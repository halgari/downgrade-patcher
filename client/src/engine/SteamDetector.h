#pragma once
#include <QObject>
#include <QStringList>
#include "api/types.h"

class SteamDetector : public QObject {
    Q_OBJECT
public:
    explicit SteamDetector(QObject *parent = nullptr);
    QList<DetectedGame> detectGames(const QList<GameConfig> &knownGames) const;
    QStringList findSteamLibraryFolders() const;
    static QStringList parseSteamLibraryFolders(const QString &vdfContent);
    static bool isAppInstalled(const QString &steamAppsDir, int appId);
private:
    QStringList defaultSteamPaths() const;
    QString findSteamRoot() const;
};
