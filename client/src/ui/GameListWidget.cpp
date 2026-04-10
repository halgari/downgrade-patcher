#include "ui/GameListWidget.h"

#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>

GameListWidget::GameListWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_updateBanner(new QLabel(this))
{
    m_updateBanner->setVisible(false);
    m_updateBanner->setStyleSheet(
        "background: #2a5c2a; color: white; padding: 8px; border-radius: 4px;"
    );
    m_layout->addWidget(m_updateBanner);
}

void GameListWidget::setUpdateBanner(const QString &version, const QString &url) {
    m_updateBanner->setText(QString("Update available (v%1) — <a href='%2' style='color: #aaffaa;'>Download from Nexus</a>").arg(version, url));
    m_updateBanner->setOpenExternalLinks(true);
    m_updateBanner->setVisible(true);
}

void GameListWidget::setGames(const QList<GameConfig> &games, const QList<DetectedGame> &detected) {
    // Clear existing game entries (keep banner)
    while (m_layout->count() > 1) {
        auto item = m_layout->takeAt(1);
        delete item->widget();
        delete item;
    }

    // Build detection lookup
    QMap<QString, QString> detectedMap;
    for (const auto &d : detected) {
        detectedMap[d.gameSlug] = d.installPath;
    }

    for (const auto &game : games) {
        auto *card = new QWidget(this);
        auto *cardLayout = new QHBoxLayout(card);
        auto *info = new QVBoxLayout();
        auto *nameLabel = new QLabel(game.name, card);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
        info->addWidget(nameLabel);

        bool isDetected = detectedMap.contains(game.slug);
        QString installPath = detectedMap.value(game.slug);

        if (isDetected) {
            auto *pathLabel = new QLabel(installPath, card);
            pathLabel->setStyleSheet("color: #aaaaaa; font-size: 11px;");
            info->addWidget(pathLabel);
            card->setStyleSheet("background: #2a2a3e; border: 1px solid #444; border-radius: 4px; padding: 12px;");
        } else {
            auto *statusLabel = new QLabel("Not installed", card);
            statusLabel->setStyleSheet("color: #888888; font-size: 11px;");
            info->addWidget(statusLabel);
            card->setStyleSheet("background: #1e1e2e; border: 1px solid #333; border-radius: 4px; padding: 12px;");
        }

        cardLayout->addLayout(info, 1);

        if (isDetected) {
            auto *btn = new QPushButton("Select \u2192", card);
            btn->setStyleSheet("padding: 6px 16px;");
            connect(btn, &QPushButton::clicked, this, [this, slug = game.slug, path = installPath]() {
                emit gameSelected(slug, path);
            });
            cardLayout->addWidget(btn);
        }

        m_layout->addWidget(card);
    }

    m_layout->addStretch();
}
