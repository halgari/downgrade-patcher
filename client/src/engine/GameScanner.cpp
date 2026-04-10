#include "engine/GameScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <xxhash.h>

GameScanner::GameScanner(QObject *parent) : QObject(parent) {}

QString GameScanner::hashFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};

    XXH3_state_t *state = XXH3_createState();
    XXH3_64bits_reset(state);

    constexpr qint64 chunkSize = 1024 * 1024; // 1MB
    QByteArray buf(chunkSize, Qt::Uninitialized);

    while (true) {
        qint64 bytesRead = file.read(buf.data(), chunkSize);
        if (bytesRead <= 0) break;
        XXH3_64bits_update(state, buf.constData(), bytesRead);
    }

    XXH64_hash_t hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);

    return QString::asprintf("%016llx", static_cast<unsigned long long>(hash));
}

ScanResult GameScanner::scan(
    const QString &installPath,
    const Manifest &targetManifest,
    const QSet<QString> &knownHashes,
    const QSet<QString> &knownPaths,
    HashCache &cache
) {
    ScanResult result;
    QDir installDir(installPath);

    // Build target lookup: path -> target hash
    QMap<QString, QString> targetByPath;
    for (const auto &f : targetManifest.files) {
        targetByPath[f.path] = f.xxhash3;
    }

    // Determine which local files to hash (only those in knownPaths)
    QStringList filesToHash;
    for (const auto &path : knownPaths) {
        QString fullPath = installDir.filePath(path);
        if (QFile::exists(fullPath)) {
            filesToHash.append(path);
        }
    }

    // Hash local files
    cache.load();
    QMap<QString, QString> localHashes; // path -> hash
    int completed = 0;
    int total = filesToHash.size();

    for (const auto &relPath : filesToHash) {
        QString fullPath = installDir.filePath(relPath);
        QFileInfo info(fullPath);
        qint64 size = info.size();
        QDateTime mtime = info.lastModified();

        QString hash = cache.lookup(relPath, size, mtime);
        if (hash.isEmpty()) {
            hash = hashFile(fullPath);
            cache.store(relPath, size, mtime, hash);
        }
        localHashes[relPath] = hash;
        ++completed;
        emit fileHashProgress(relPath, completed, total);
    }
    cache.save();

    // Categorize files
    // 1. Files in target manifest
    for (const auto &targetFile : targetManifest.files) {
        auto localIt = localHashes.find(targetFile.path);
        if (localIt == localHashes.end()) {
            // File in target but not on disk
            result.entries.append({targetFile.path, ScanCategory::Missing, {}, targetFile.xxhash3});
        } else if (localIt.value() == targetFile.xxhash3) {
            result.entries.append({targetFile.path, ScanCategory::Unchanged, localIt.value(), targetFile.xxhash3});
        } else if (knownHashes.contains(localIt.value())) {
            result.entries.append({targetFile.path, ScanCategory::Patchable, localIt.value(), targetFile.xxhash3});
        } else {
            result.entries.append({targetFile.path, ScanCategory::Unknown, localIt.value(), targetFile.xxhash3});
        }
    }

    // 2. Files on disk but not in target (Extra)
    for (auto it = localHashes.begin(); it != localHashes.end(); ++it) {
        if (!targetByPath.contains(it.key())) {
            result.entries.append({it.key(), ScanCategory::Extra, it.value(), {}});
        }
    }

    return result;
}
