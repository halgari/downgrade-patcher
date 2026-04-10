#include "engine/Patcher.h"
#include "api/ApiClient.h"
#include "engine/GameScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <zstd.h>

Patcher::Patcher(ApiClient *apiClient, int maxConcurrentChunks, QObject *parent)
    : QObject(parent)
    , m_apiClient(apiClient)
    , m_maxConcurrentChunks(maxConcurrentChunks)
    , m_chunksReceived(0)
    , m_chunksRequested(0)
    , m_inFlightRequests(0)
    , m_filesCompleted(0)
    , m_totalFiles(0)
    , m_successCount(0)
    , m_failCount(0)
{
}

QByteArray Patcher::decompressChunk(const QByteArray &patchData, const QByteArray &dictData)
{
    if (patchData.isEmpty()) {
        return {};
    }

    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    if (!dctx) {
        return {};
    }

    ZSTD_DDict *ddict = nullptr;
    if (!dictData.isEmpty()) {
        ddict = ZSTD_createDDict(dictData.constData(), dictData.size());
        if (!ddict) {
            ZSTD_freeDCtx(dctx);
            return {};
        }
        size_t ret = ZSTD_DCtx_refDDict(dctx, ddict);
        if (ZSTD_isError(ret)) {
            ZSTD_freeDDict(ddict);
            ZSTD_freeDCtx(dctx);
            return {};
        }
    }

    // Try to get exact decompressed size; fall back to 8MB max
    unsigned long long frameSize = ZSTD_getFrameContentSize(patchData.constData(), patchData.size());
    size_t outCapacity;
    if (frameSize == ZSTD_CONTENTSIZE_UNKNOWN || frameSize == ZSTD_CONTENTSIZE_ERROR) {
        outCapacity = 8 * 1024 * 1024;
    } else {
        outCapacity = static_cast<size_t>(frameSize);
    }

    QByteArray output(static_cast<qsizetype>(outCapacity), Qt::Uninitialized);

    size_t decompSize = ZSTD_decompressDCtx(dctx, output.data(), output.size(),
                                              patchData.constData(), patchData.size());

    if (ddict) {
        ZSTD_freeDDict(ddict);
    }
    ZSTD_freeDCtx(dctx);

    if (ZSTD_isError(decompSize)) {
        return {};
    }

    output.resize(static_cast<qsizetype>(decompSize));
    return output;
}

QString Patcher::findClosestDonor(const QString &targetPath, const QStringList &localFiles)
{
    // Exact path match first
    for (const auto &local : localFiles) {
        if (local == targetPath) {
            return local;
        }
    }

    // Filename match
    QString targetFilename = QFileInfo(targetPath).fileName();
    for (const auto &local : localFiles) {
        if (QFileInfo(local).fileName() == targetFilename) {
            return local;
        }
    }

    return {};
}

void Patcher::start(const QString &gameSlug, const QString &installPath, const ScanResult &scanResult)
{
    m_gameSlug = gameSlug;
    m_installPath = installPath;
    m_fileQueue.clear();
    m_filesCompleted = 0;
    m_successCount = 0;
    m_failCount = 0;
    m_failedFiles.clear();

    // Collect patchable and missing entries
    for (const auto &entry : scanResult.entries) {
        if (entry.category == ScanCategory::Patchable || entry.category == ScanCategory::Missing) {
            m_fileQueue.append(entry);
        }
    }

    // Delete extra files
    deleteExtraFiles(installPath, scanResult.entries);

    m_totalFiles = m_fileQueue.size();

    if (m_fileQueue.isEmpty()) {
        emit finished(PatchSummary{0, 0, {}});
        return;
    }

    processNextFile();
}

void Patcher::deleteExtraFiles(const QString &installPath, const QList<ScanEntry> &entries)
{
    for (const auto &entry : entries) {
        if (entry.category == ScanCategory::Extra) {
            QString fullPath = installPath + "/" + entry.path;
            QFile::remove(fullPath);
        }
    }
}

void Patcher::processNextFile()
{
    if (m_fileQueue.isEmpty()) {
        emit finished(PatchSummary{m_successCount, m_failCount, m_failedFiles});
        return;
    }

    ScanEntry entry = m_fileQueue.takeFirst();
    startFilePatching(entry);
}

void Patcher::startFilePatching(const ScanEntry &entry)
{
    m_currentEntry = entry;
    m_chunkData.clear();
    m_chunksReceived = 0;
    m_chunksRequested = 0;
    m_inFlightRequests = 0;
    m_currentMeta = {};

    emit fileStarted(entry.path);

    // For missing files, find a donor
    if (entry.category == ScanCategory::Missing) {
        QStringList localFiles;
        // We don't have a full local file list here, so donor path stays empty
        // for missing files unless we can match by existing files in the install dir
        m_donorPath = {};
    } else {
        m_donorPath = m_installPath + "/" + entry.path;
    }

    // Disconnect previous connections
    QObject::disconnect(m_metaConn);
    QObject::disconnect(m_chunkConn);
    QObject::disconnect(m_errorConn);

    // Connect for this file
    m_metaConn = connect(m_apiClient, &ApiClient::patchMetaReady,
                          this, &Patcher::onPatchMetaReady);
    m_errorConn = connect(m_apiClient, &ApiClient::errorOccurred,
                          this, [this](const QString &error) {
        QObject::disconnect(m_metaConn);
        QObject::disconnect(m_chunkConn);
        QObject::disconnect(m_errorConn);
        emit fileFailed(m_currentEntry.path, error);
        m_failCount++;
        m_failedFiles.append(m_currentEntry.path);
        m_filesCompleted++;
        emit progressUpdated(PatchProgress{m_filesCompleted, m_totalFiles, 0, 0, m_currentEntry.path});
        processNextFile();
    });

    // Request patch meta
    QString sourceHash = entry.localHash.isEmpty() ? QStringLiteral("empty") : entry.localHash;
    m_apiClient->fetchPatchMeta(m_gameSlug, sourceHash, entry.targetHash);
}

void Patcher::onPatchMetaReady(const PatchMeta &meta)
{
    QObject::disconnect(m_metaConn);

    m_currentMeta = meta;

    m_chunkConn = connect(m_apiClient, &ApiClient::patchChunkReady,
                           this, &Patcher::onPatchChunkReady);

    requestMoreChunks();
}

void Patcher::requestMoreChunks()
{
    while (m_inFlightRequests < m_maxConcurrentChunks && m_chunksRequested < m_currentMeta.totalChunks) {
        int idx = m_chunksRequested;
        m_chunksRequested++;
        m_inFlightRequests++;

        QString sourceHash = m_currentEntry.localHash.isEmpty() ? QStringLiteral("empty") : m_currentEntry.localHash;
        m_apiClient->fetchPatchChunk(m_gameSlug, sourceHash, m_currentEntry.targetHash, idx);
    }
}

void Patcher::onPatchChunkReady(int chunkIndex, const QByteArray &data)
{
    m_chunkData[chunkIndex] = data;
    m_chunksReceived++;
    m_inFlightRequests--;

    emit progressUpdated(PatchProgress{
        m_filesCompleted, m_totalFiles,
        m_chunksReceived, m_currentMeta.totalChunks,
        m_currentEntry.path
    });

    if (m_chunksReceived == m_currentMeta.totalChunks) {
        QObject::disconnect(m_chunkConn);
        QObject::disconnect(m_errorConn);
        finalizeCurrentFile();
    } else {
        requestMoreChunks();
    }
}

void Patcher::finalizeCurrentFile()
{
    QString outputPath = m_installPath + "/" + m_currentEntry.path;

    // Read local source file as dictionary (for zstd dictionary decompression)
    QByteArray dictData;
    if (!m_donorPath.isEmpty() && QFile::exists(m_donorPath)) {
        QFile donorFile(m_donorPath);
        if (donorFile.open(QIODevice::ReadOnly)) {
            dictData = donorFile.readAll();
        }
    }

    // Decompress all chunks in order and concatenate
    QByteArray fullOutput;
    for (int i = 0; i < m_currentMeta.totalChunks; ++i) {
        QByteArray decompressed = decompressChunk(m_chunkData[i], dictData);
        if (decompressed.isEmpty() && !m_chunkData[i].isEmpty()) {
            emit fileFailed(m_currentEntry.path, QStringLiteral("Decompression failed for chunk %1").arg(i));
            m_failCount++;
            m_failedFiles.append(m_currentEntry.path);
            m_filesCompleted++;
            processNextFile();
            return;
        }
        fullOutput.append(decompressed);
    }

    // Write to temp file
    QString tempPath = outputPath + ".tmp";
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly)) {
            emit fileFailed(m_currentEntry.path, QStringLiteral("Failed to open temp file for writing"));
            m_failCount++;
            m_failedFiles.append(m_currentEntry.path);
            m_filesCompleted++;
            processNextFile();
            return;
        }
        tempFile.write(fullOutput);
    }

    // Verify xxhash3
    QString hash = GameScanner::hashFile(tempPath);
    if (hash != m_currentEntry.targetHash) {
        QFile::remove(tempPath);
        emit fileFailed(m_currentEntry.path,
                         QStringLiteral("Hash mismatch: expected %1, got %2")
                             .arg(m_currentEntry.targetHash, hash));
        m_failCount++;
        m_failedFiles.append(m_currentEntry.path);
        m_filesCompleted++;
        processNextFile();
        return;
    }

    // Atomic rename
    if (QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }
    if (!QFile::rename(tempPath, outputPath)) {
        QFile::remove(tempPath);
        emit fileFailed(m_currentEntry.path, QStringLiteral("Failed to rename temp file"));
        m_failCount++;
        m_failedFiles.append(m_currentEntry.path);
        m_filesCompleted++;
        processNextFile();
        return;
    }

    m_successCount++;
    m_filesCompleted++;
    emit fileCompleted(m_currentEntry.path);
    emit progressUpdated(PatchProgress{m_filesCompleted, m_totalFiles, 0, 0, m_currentEntry.path});
    processNextFile();
}
