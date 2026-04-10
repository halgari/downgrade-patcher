#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include "api/ApiClient.h"
#include "engine/SteamDetector.h"
#include "ui/GameListWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ApiClient *apiClient, QWidget *parent = nullptr);

private slots:
    void onGamesReady(const QList<GameConfig> &games);
    void onGameSelected(const QString &gameSlug, const QString &installPath);

private:
    ApiClient *m_apiClient;
    SteamDetector m_steamDetector;
    QStackedWidget *m_stack;
    GameListWidget *m_gameList;
    QList<GameConfig> m_games;
};
