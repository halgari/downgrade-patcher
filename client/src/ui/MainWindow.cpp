#include "ui/MainWindow.h"

MainWindow::MainWindow(ApiClient *apiClient, QWidget *parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
    , m_stack(new QStackedWidget(this))
    , m_gameList(new GameListWidget(this))
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

    m_stack->addWidget(m_gameList); // index 0

    connect(m_apiClient, &ApiClient::gamesReady, this, &MainWindow::onGamesReady);
    connect(m_gameList, &GameListWidget::gameSelected, this, &MainWindow::onGameSelected);

    // Fetch game list on startup
    m_apiClient->fetchGames();
}

void MainWindow::onGamesReady(const QList<GameConfig> &games) {
    m_games = games;
    auto detected = m_steamDetector.detectGames(games);
    m_gameList->setGames(games, detected);
}

void MainWindow::onGameSelected(const QString &gameSlug, const QString &installPath) {
    // PatchWidget will be added in Task 9
    Q_UNUSED(gameSlug);
    Q_UNUSED(installPath);
}
