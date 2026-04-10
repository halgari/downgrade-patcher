#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include "api/types.h"

class GameListWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameListWidget(QWidget *parent = nullptr);

    void setGames(const QList<GameConfig> &games, const QList<DetectedGame> &detected);
    void setUpdateBanner(const QString &version, const QString &url);

signals:
    void gameSelected(const QString &gameSlug, const QString &installPath);

private:
    QVBoxLayout *m_layout;
    QLabel *m_updateBanner;
};
