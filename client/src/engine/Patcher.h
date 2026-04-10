#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include "api/types.h"

class ApiClient;

struct PatchProgress {
    int filesCompleted;
    int totalFiles;
    int currentFileChunksCompleted;
    int currentFileTotalChunks;
    QString currentFile;
};

struct PatchSummary {
    int successCount;
    int failCount;
    QStringList failedFiles;
};

class Patcher : public QObject {
    Q_OBJECT

public:
    explicit Patcher(ApiClient *apiClient, int maxConcurrentChunks = 4, QObject *parent = nullptr);

    void start(const QString &gameSlug, const QString &installPath, const ScanResult &scanResult);

    static QByteArray decompressChunk(const QByteArray &patchData, const QByteArray &dictData);
    static QString findClosestDonor(const QString &targetPath, const QStringList &localFiles);

signals:
    void progressUpdated(PatchProgress progress);
    void fileStarted(const QString &path);
    void fileCompleted(const QString &path);
    void fileFailed(const QString &path, const QString &error);
    void finished(PatchSummary summary);

private:
    void processNextFile();
    void startFilePatching(const ScanEntry &entry);
    void onPatchMetaReady(const PatchMeta &meta);
    void onPatchChunkReady(int chunkIndex, const QByteArray &data);
    void requestMoreChunks();
    void finalizeCurrentFile();
    void deleteExtraFiles(const QString &installPath, const QList<ScanEntry> &entries);

    ApiClient *m_apiClient;
    int m_maxConcurrentChunks;
    QString m_gameSlug;
    QString m_installPath;

    QList<ScanEntry> m_fileQueue;
    ScanEntry m_currentEntry;
    QString m_donorPath;
    PatchMeta m_currentMeta;
    QMap<int, QByteArray> m_chunkData;
    int m_chunksReceived;
    int m_chunksRequested;
    int m_inFlightRequests;

    int m_filesCompleted;
    int m_totalFiles;
    int m_successCount;
    int m_failCount;
    QStringList m_failedFiles;

    QMetaObject::Connection m_metaConn;
    QMetaObject::Connection m_chunkConn;
    QMetaObject::Connection m_errorConn;
};
