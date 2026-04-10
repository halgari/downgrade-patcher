#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QDateTime>

struct CachedHash {
    QString path;
    qint64 size;
    QDateTime mtime;
    QString xxhash3;
};

class HashCache {
public:
    explicit HashCache(const QString &cacheFilePath);

    QString lookup(const QString &relativePath, qint64 size, const QDateTime &mtime) const;
    void store(const QString &relativePath, qint64 size, const QDateTime &mtime, const QString &hash);
    void load();
    void save() const;

private:
    QString m_cacheFilePath;
    QMap<QString, CachedHash> m_entries;
};
