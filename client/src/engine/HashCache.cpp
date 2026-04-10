#include "engine/HashCache.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

HashCache::HashCache(const QString &cacheFilePath) : m_cacheFilePath(cacheFilePath) {}

QString HashCache::lookup(const QString &relativePath, qint64 size, const QDateTime &mtime) const {
    auto it = m_entries.find(relativePath);
    if (it == m_entries.end()) return {};
    const auto &entry = it.value();
    if (entry.size == size && entry.mtime == mtime) return entry.xxhash3;
    return {};
}

void HashCache::store(const QString &relativePath, qint64 size, const QDateTime &mtime, const QString &hash) {
    m_entries[relativePath] = CachedHash{relativePath, size, mtime, hash};
}

void HashCache::load() {
    QFile file(m_cacheFilePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(file.readAll());
    for (const auto &val : doc.array()) {
        auto obj = val.toObject();
        CachedHash entry;
        entry.path = obj["path"].toString();
        entry.size = obj["size"].toInteger();
        entry.mtime = QDateTime::fromMSecsSinceEpoch(obj["mtime"].toInteger());
        entry.xxhash3 = obj["xxhash3"].toString();
        m_entries[entry.path] = entry;
    }
}

void HashCache::save() const {
    QJsonArray arr;
    for (const auto &entry : m_entries) {
        QJsonObject obj;
        obj["path"] = entry.path;
        obj["size"] = entry.size;
        obj["mtime"] = entry.mtime.toMSecsSinceEpoch();
        obj["xxhash3"] = entry.xxhash3;
        arr.append(obj);
    }
    QFile file(m_cacheFilePath);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(arr).toJson());
}
