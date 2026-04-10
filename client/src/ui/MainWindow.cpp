#include "ui/MainWindow.h"

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
}

void MainWindow::onGameSelected(const QString &gameSlug, const QString &installPath) {
    m_patchWidget->setGame(gameSlug, installPath, m_games);
    m_stack->setCurrentIndex(1);
}

void MainWindow::onBackToGameList() {
    m_stack->setCurrentIndex(0);
}
