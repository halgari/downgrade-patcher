#include "ui/MainWindow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCoreApplication>

#define NEXUS_API_KEY "NEXUS_API_KEY_PLACEHOLDER"

MainWindow::MainWindow(ApiClient *apiClient, QWidget *parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
    , m_stack(new QStackedWidget(this))
    , m_gameList(new GameListWidget(this))
    , m_patchWidget(new PatchWidget(apiClient, this))
{
    setWindowTitle("Downgrade Patcher");
    setMinimumSize(500, 400);
    setCentralWidget(m_stack);

    // Dark theme
    setStyleSheet(
        "QMainWindow { background: #1a1a2e; }"
        "QWidget { color: #dddddd; }"
        "QPushButton { background: #3a3a5e; border: 1px solid #555; border-radius: 4px; padding: 6px 12px; color: #eee; }"
        "QPushButton:hover { background: #4a4a6e; }"
        "QComboBox { background: #2a2a3e; border: 1px solid #555; padding: 4px 8px; color: #eee; }"
    );

    m_stack->addWidget(m_gameList);    // index 0
    m_stack->addWidget(m_patchWidget); // index 1

    connect(m_apiClient, &ApiClient::gamesReady, this, &MainWindow::onGamesReady);
    connect(m_gameList, &GameListWidget::gameSelected, this, &MainWindow::onGameSelected);
    connect(m_patchWidget, &PatchWidget::backRequested, this, &MainWindow::onBackToGameList);

    // Fetch game list on startup
    m_apiClient->fetchGames();
}

void MainWindow::onGamesReady(const QList<GameConfig> &games) {
    m_games = games;
    auto detected = m_steamDetector.detectGames(games);
    m_gameList->setGames(games, detected);
    checkForUpdate();
}

void MainWindow::checkForUpdate() {
    if (m_games.isEmpty()) {
        return;
    }

    const auto &game = m_games.first();
    QString url = QString("https://api.nexusmods.com/v1/games/%1/mods/%2/files.json")
        .arg(game.nexusDomain)
        .arg(game.nexusModId);

    QUrl reqUrl(url);
    QNetworkRequest req(reqUrl);
    req.setRawHeader("apikey", NEXUS_API_KEY);
    req.setRawHeader("accept", "application/json");

    auto *reply = m_updateNam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onUpdateCheckFinished(reply);
    });
}

void MainWindow::onUpdateCheckFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return;
    }

    auto doc = QJsonDocument::fromJson(reply->readAll());
    auto files = doc.object()["files"].toArray();
    reply->deleteLater();

    if (files.isEmpty()) return;

    QString latestVersion;
    QString latestUrl;
    qint64 latestDate = 0;

    for (const auto &val : files) {
        auto obj = val.toObject();
        qint64 uploaded = obj["uploaded_timestamp"].toInteger();
        if (uploaded > latestDate) {
            latestDate = uploaded;
            latestVersion = obj["version"].toString();
        }
    }

    if (latestVersion.isEmpty()) return;

    if (latestVersion != QCoreApplication::applicationVersion()) {
        if (!m_games.isEmpty()) {
            const auto &game = m_games.first();
            latestUrl = QString("https://www.nexusmods.com/%1/mods/%2")
                .arg(game.nexusDomain)
                .arg(game.nexusModId);
        }
        m_gameList->setUpdateBanner(latestVersion, latestUrl);
    }
}

void MainWindow::onGameSelected(const QString &gameSlug, const QString &installPath) {
    m_patchWidget->setGame(gameSlug, installPath, m_games);
    m_stack->setCurrentIndex(1);
}

void MainWindow::onBackToGameList() {
    m_stack->setCurrentIndex(0);
}
