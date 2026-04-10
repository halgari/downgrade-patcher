// client/src/api/ApiClient.cpp
#include "api/ApiClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>

ApiClient::ApiClient(const QUrl &baseUrl, QObject *parent)
    : QObject(parent), m_baseUrl(baseUrl) {}

QUrl ApiClient::buildUrl(const QString &path) const {
    QUrl url = m_baseUrl;
    url.setPath(url.path() + path);
    return url;
}

void ApiClient::handleNetworkError(QNetworkReply *reply) {
    emit errorOccurred(
        QString("Network error: %1 (HTTP %2)")
            .arg(reply->errorString())
            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
    );
    reply->deleteLater();
}

void ApiClient::fetchGames() {
    auto *reply = m_nam.get(QNetworkRequest(buildUrl("/api/games")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            handleNetworkError(reply);
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto games = parseGames(doc.object()["games"].toArray());
        emit gamesReady(games);
        reply->deleteLater();
    });
}

void ApiClient::fetchManifestIndex(const QString &gameSlug) {
    auto *reply = m_nam.get(QNetworkRequest(buildUrl(QString("/api/%1/manifest/index").arg(gameSlug))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, gameSlug]() {
        if (reply->error() != QNetworkReply::NoError) {
            handleNetworkError(reply);
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto index = parseManifestIndex(doc.object());
        emit manifestIndexReady(gameSlug, index);
        reply->deleteLater();
    });
}

void ApiClient::fetchManifest(const QString &gameSlug, const QString &version) {
    auto *reply = m_nam.get(QNetworkRequest(buildUrl(QString("/api/%1/manifest/%2").arg(gameSlug, version))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, gameSlug]() {
        if (reply->error() != QNetworkReply::NoError) {
            handleNetworkError(reply);
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto manifest = parseManifest(doc.object());
        emit manifestReady(gameSlug, manifest);
        reply->deleteLater();
    });
}

void ApiClient::fetchPatchMeta(const QString &gameSlug, const QString &sourceHash, const QString &targetHash) {
    auto path = QString("/api/%1/patch/%2/%3/meta").arg(gameSlug, sourceHash, targetHash);
    auto *reply = m_nam.get(QNetworkRequest(buildUrl(path)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            handleNetworkError(reply);
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto meta = parsePatchMeta(doc.object());
        emit patchMetaReady(meta);
        reply->deleteLater();
    });
}

void ApiClient::fetchPatchChunk(const QString &gameSlug, const QString &sourceHash, const QString &targetHash, int chunkIndex) {
    auto path = QString("/api/%1/patch/%2/%3/%4").arg(gameSlug, sourceHash, targetHash, QString::number(chunkIndex));
    auto *reply = m_nam.get(QNetworkRequest(buildUrl(path)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, chunkIndex]() {
        if (reply->error() != QNetworkReply::NoError) {
            handleNetworkError(reply);
            return;
        }
        emit patchChunkReady(chunkIndex, reply->readAll());
        reply->deleteLater();
    });
}

QList<GameConfig> ApiClient::parseGames(const QJsonArray &arr) {
    QList<GameConfig> games;
    for (const auto &val : arr) {
        auto obj = val.toObject();
        GameConfig g;
        g.slug = obj["slug"].toString();
        g.name = obj["name"].toString();
        g.steamAppId = obj["steam_app_id"].toInt();
        for (const auto &d : obj["depot_ids"].toArray())
            g.depotIds.append(d.toInt());
        g.exePath = obj["exe_path"].toString();
        g.bestOfBothWorlds = obj["best_of_both_worlds"].toBool();
        g.nexusDomain = obj["nexus_domain"].toString();
        g.nexusModId = obj["nexus_mod_id"].toInt();
        games.append(g);
    }
    return games;
}

ManifestIndex ApiClient::parseManifestIndex(const QJsonObject &obj) {
    ManifestIndex idx;
    idx.game = obj["game"].toString();
    auto versions = obj["versions"].toObject();
    for (auto it = versions.begin(); it != versions.end(); ++it) {
        idx.versions[it.key()] = it.value().toString();
    }
    return idx;
}

Manifest ApiClient::parseManifest(const QJsonObject &obj) {
    Manifest m;
    m.game = obj["game"].toString();
    m.version = obj["version"].toString();
    for (const auto &val : obj["files"].toArray()) {
        auto fobj = val.toObject();
        FileEntry f;
        f.path = fobj["path"].toString();
        f.size = fobj["size"].toInteger();
        f.xxhash3 = fobj["xxhash3"].toString();
        m.files.append(f);
    }
    return m;
}

PatchMeta ApiClient::parsePatchMeta(const QJsonObject &obj) {
    PatchMeta meta;
    meta.totalChunks = obj["total_chunks"].toInt();
    for (const auto &val : obj["chunks"].toArray()) {
        auto cobj = val.toObject();
        ChunkMeta c;
        c.index = cobj["index"].toInt();
        c.size = cobj["size"].toInteger();
        meta.chunks.append(c);
    }
    return meta;
}
