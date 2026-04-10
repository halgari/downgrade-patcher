#pragma once
#include <QObject>
#include <QSet>
#include "api/types.h"
#include "engine/HashCache.h"

class GameScanner : public QObject {
    Q_OBJECT
public:
    explicit GameScanner(QObject *parent = nullptr);

    ScanResult scan(
        const QString &installPath,
        const Manifest &targetManifest,
        const QSet<QString> &knownHashes,
        const QSet<QString> &knownPaths,
        HashCache &cache
    );

    static QString hashFile(const QString &filePath);

signals:
    void fileHashProgress(const QString &currentFile, int filesCompleted, int totalFiles);
};
