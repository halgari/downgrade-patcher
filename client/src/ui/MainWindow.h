#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QNetworkAccessManager>
#include "api/ApiClient.h"
#include "engine/SteamDetector.h"
#include "ui/GameListWidget.h"
#include "ui/PatchWidget.h"

class QNetworkReply;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ApiClient *apiClient, QWidget *parent = nullptr);

private slots:
    void onGamesReady(const QList<GameConfig> &games);
    void onGameSelected(const QString &gameSlug, const QString &installPath);
    void onBackToGameList();
    void onConnectionError(const QString &message);

private:
    ApiClient *m_apiClient;
    SteamDetector m_steamDetector;
    QStackedWidget *m_stack;
    GameListWidget *m_gameList;
    PatchWidget *m_patchWidget;
    QList<GameConfig> m_games;

    void checkForUpdate();
    void onUpdateCheckFinished(QNetworkReply *reply);
    QNetworkAccessManager m_updateNam;
};
