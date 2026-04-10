#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include "api/types.h"

class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(const QUrl &baseUrl, QObject *parent = nullptr);

    void fetchGames();
    void fetchManifestIndex(const QString &gameSlug);
    void fetchManifest(const QString &gameSlug, const QString &version);
    void fetchPatchMeta(const QString &gameSlug, const QString &sourceHash, const QString &targetHash);
    void fetchPatchChunk(const QString &gameSlug, const QString &sourceHash, const QString &targetHash, int chunkIndex);

signals:
    void gamesReady(const QList<GameConfig> &games);
    void manifestIndexReady(const QString &gameSlug, const ManifestIndex &index);
    void manifestReady(const QString &gameSlug, const Manifest &manifest);
    void patchMetaReady(const PatchMeta &meta);
    void patchChunkReady(int chunkIndex, const QByteArray &data);
    void errorOccurred(const QString &message);

private:
    QNetworkAccessManager m_nam;
    QUrl m_baseUrl;

    QUrl buildUrl(const QString &path) const;
    void handleNetworkError(QNetworkReply *reply);

    static QList<GameConfig> parseGames(const QJsonArray &arr);
    static ManifestIndex parseManifestIndex(const QJsonObject &obj);
    static Manifest parseManifest(const QJsonObject &obj);
    static PatchMeta parsePatchMeta(const QJsonObject &obj);
};
