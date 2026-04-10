#pragma once

#include <QString>
#include <QList>
#include <QMap>

struct GameConfig {
    QString slug;
    QString name;
    int steamAppId;
    QList<int> depotIds;
    QString exePath;
    bool bestOfBothWorlds;
    QString nexusDomain;
    int nexusModId;
};

struct FileEntry {
    QString path;
    qint64 size;
    QString xxhash3;
};

struct Manifest {
    QString game;
    QString version;
    QList<FileEntry> files;
};

struct ManifestIndex {
    QString game;
    QMap<QString, QString> versions; // hash -> version
};

struct ChunkMeta {
    int index;
    qint64 size;
};

struct PatchMeta {
    int totalChunks;
    QList<ChunkMeta> chunks;
};

struct DetectedGame {
    QString gameSlug;
    QString installPath;
};

enum class ScanCategory {
    Unchanged,
    Patchable,
    Unknown,
    Missing,
    Extra,
};

struct ScanEntry {
    QString path;
    ScanCategory category;
    QString localHash;
    QString targetHash;
};

struct ScanResult {
    QList<ScanEntry> entries;

    int countByCategory(ScanCategory cat) const {
        int count = 0;
        for (const auto &e : entries) {
            if (e.category == cat) ++count;
        }
        return count;
    }
};
