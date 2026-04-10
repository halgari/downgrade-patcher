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
    setMinimumSize(600, 500);
    setCentralWidget(m_stack);

    // Fantasy parchment theme
    setStyleSheet(R"(
        QMainWindow {
            background: #1c1410;
        }
        QWidget {
            color: #d4c4a0;
            font-family: "Palatino Linotype", "Book Antiqua", Palatino, Georgia, serif;
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5c3d2e, stop:1 #3d2518);
            border: 2px solid #8b6914;
            border-radius: 3px;
            padding: 8px 16px;
            color: #e8d5a3;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #7a5240, stop:1 #5c3d2e);
            border-color: #c4972a;
        }
        QPushButton:pressed {
            background: #2a1a10;
        }
        QPushButton:disabled {
            background: #2a1a10;
            border-color: #4a3a2a;
            color: #6a5a4a;
        }
        QComboBox {
            background: #2a1c14;
            border: 2px solid #6b4c1e;
            border-radius: 3px;
            padding: 6px 10px;
            color: #e8d5a3;
            font-size: 13px;
            min-height: 20px;
        }
        QComboBox:hover {
            border-color: #8b6914;
        }
        QComboBox::drop-down {
            border-left: 1px solid #6b4c1e;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: #2a1c14;
            border: 2px solid #6b4c1e;
            color: #e8d5a3;
            selection-background-color: #5c3d2e;
        }
        QProgressBar {
            background: #1a1008;
            border: 2px solid #6b4c1e;
            border-radius: 4px;
            text-align: center;
            color: #e8d5a3;
            font-size: 11px;
            min-height: 18px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #8b6914, stop:0.5 #c4972a, stop:1 #8b6914);
            border-radius: 2px;
        }
        QTextEdit {
            background: #140e08;
            border: 2px solid #4a3520;
            border-radius: 4px;
            color: #b0a080;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 11px;
            padding: 4px;
        }
        QCheckBox {
            color: #d4c4a0;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 2px solid #6b4c1e;
            border-radius: 2px;
            background: #2a1c14;
        }
        QCheckBox::indicator:checked {
            background: #8b6914;
        }
        QLineEdit {
            background: #2a1c14;
            border: 2px solid #6b4c1e;
            border-radius: 3px;
            padding: 6px;
            color: #e8d5a3;
        }
        QScrollBar:vertical {
            background: #1a1008;
            width: 10px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #5c3d2e;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");

    m_stack->addWidget(m_gameList);    // index 0
    m_stack->addWidget(m_patchWidget); // index 1

    connect(m_apiClient, &ApiClient::gamesReady, this, &MainWindow::onGamesReady);
    connect(m_apiClient, &ApiClient::errorOccurred, this, &MainWindow::onConnectionError);
    connect(m_gameList, &GameListWidget::gameSelected, this, &MainWindow::onGameSelected);
    connect(m_patchWidget, &PatchWidget::backRequested, this, &MainWindow::onBackToGameList);

    // Show loading state, then fetch game list
    m_gameList->showStatus("Connecting to server...");
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

void MainWindow::onConnectionError(const QString &message) {
    m_gameList->showStatus("Could not connect to server.\n\n" + message + "\n\nCheck that the patch server is running.");
}
