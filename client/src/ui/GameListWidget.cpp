#include "ui/GameListWidget.h"
#include "ui/SettingsDialog.h"

#include <QHBoxLayout>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFrame>

GameListWidget::GameListWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_updateBanner(new QLabel(this))
{
    m_layout->setContentsMargins(24, 16, 24, 16);
    m_layout->setSpacing(12);

    m_updateBanner->setVisible(false);
    m_updateBanner->setStyleSheet(
        "background: #2a3a1a; border: 2px solid #4a6a2a; border-radius: 4px; "
        "color: #c4d4a0; padding: 10px; font-size: 12px;"
    );
    m_layout->addWidget(m_updateBanner);
}

void GameListWidget::setUpdateBanner(const QString &version, const QString &url) {
    m_updateBanner->setText(
        QString("<b>Update available!</b> Version %1 — "
                "<a href='%2' style='color: #c4972a;'>Download from Nexus</a>")
            .arg(version, url));
    m_updateBanner->setOpenExternalLinks(true);
    m_updateBanner->setVisible(true);
}

void GameListWidget::showStatus(const QString &message) {
    while (m_layout->count() > 1) {
        auto item = m_layout->takeAt(1);
        delete item->widget();
        delete item;
    }

    auto *label = new QLabel(message, this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("color: #8a7a5a; font-size: 14px; padding: 60px 40px; font-style: italic;");
    label->setWordWrap(true);
    m_layout->addWidget(label);
    m_layout->addStretch();
}

void GameListWidget::setGames(const QList<GameConfig> &games, const QList<DetectedGame> &detected) {
    while (m_layout->count() > 1) {
        auto item = m_layout->takeAt(1);
        delete item->widget();
        delete item;
    }

    // Title banner
    auto *titleWidget = new QWidget(this);
    auto *titleLayout = new QVBoxLayout(titleWidget);
    titleLayout->setContentsMargins(0, 8, 0, 8);

    auto *titleLabel = new QLabel("Downgrade Patcher", titleWidget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "font-size: 28px; font-weight: bold; color: #c4972a; "
        "letter-spacing: 3px; padding: 0; margin: 0;"
    );
    titleLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("Select a game to patch", titleWidget);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet(
        "font-size: 12px; color: #8a7a5a; font-style: italic; "
        "padding: 0; margin: 0;"
    );
    titleLayout->addWidget(subtitleLabel);

    // Decorative divider
    auto *divider = new QFrame(titleWidget);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 transparent, stop:0.2 #6b4c1e, stop:0.5 #c4972a, "
        "stop:0.8 #6b4c1e, stop:1 transparent); "
        "border: none; max-height: 2px; margin: 8px 40px;"
    );
    titleLayout->addWidget(divider);

    m_layout->addWidget(titleWidget);

    // Build detection lookup
    QMap<QString, QString> detectedMap;
    for (const auto &d : detected) {
        detectedMap[d.gameSlug] = d.installPath;
    }

    for (const auto &game : games) {
        auto *card = new QWidget(this);
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        auto *info = new QVBoxLayout();
        info->setSpacing(4);

        bool isDetected = detectedMap.contains(game.slug);
        QString installPath = detectedMap.value(game.slug);

        auto *nameLabel = new QLabel(game.name, card);

        if (isDetected) {
            nameLabel->setStyleSheet(
                "font-weight: bold; font-size: 16px; color: #e8d5a3;"
            );
            info->addWidget(nameLabel);

            auto *pathLabel = new QLabel(installPath, card);
            pathLabel->setStyleSheet("color: #7a6a4a; font-size: 11px;");
            info->addWidget(pathLabel);

            card->setStyleSheet(
                "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "stop:0 #2e2018, stop:1 #241810); "
                "border: 2px solid #5c3d2e; border-radius: 6px;"
            );
        } else {
            nameLabel->setStyleSheet(
                "font-weight: bold; font-size: 16px; color: #5a4a3a;"
            );
            info->addWidget(nameLabel);

            auto *statusLabel = new QLabel("Not installed", card);
            statusLabel->setStyleSheet("color: #4a3a2a; font-size: 11px; font-style: italic;");
            info->addWidget(statusLabel);

            card->setStyleSheet(
                "background: #1a1208; "
                "border: 1px solid #3a2a1a; border-radius: 6px;"
            );
        }

        cardLayout->addLayout(info, 1);

        if (isDetected) {
            auto *btn = new QPushButton("Enter \u2192", card);
            connect(btn, &QPushButton::clicked, this, [this, slug = game.slug, path = installPath]() {
                emit gameSelected(slug, path);
            });
            cardLayout->addWidget(btn);
        }

        m_layout->addWidget(card);
    }

    // Footer
    auto *footerDivider = new QFrame(this);
    footerDivider->setFrameShape(QFrame::HLine);
    footerDivider->setStyleSheet(
        "background: #3a2a1a; border: none; max-height: 1px; margin: 4px 0;"
    );
    m_layout->addWidget(footerDivider);

    auto *footer = new QWidget(this);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 4, 0, 0);

    auto *settingsBtn = new QPushButton("Settings", footer);
    settingsBtn->setStyleSheet(
        "border: none; background: transparent; color: #6a5a3a; "
        "font-size: 11px; padding: 4px 8px;"
    );
    connect(settingsBtn, &QPushButton::clicked, this, [this]() {
        SettingsDialog dialog(this);
        dialog.exec();
    });
    footerLayout->addWidget(settingsBtn);
    footerLayout->addStretch();

    auto *versionLabel = new QLabel("v" + QCoreApplication::applicationVersion(), footer);
    versionLabel->setStyleSheet("color: #3a2a1a; font-size: 10px;");
    footerLayout->addWidget(versionLabel);

    m_layout->addWidget(footer);
    m_layout->addStretch();
}
