#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include "api/ApiClient.h"
#include "api/types.h"
#include "engine/GameScanner.h"
#include "engine/Patcher.h"

class PatchWidget : public QWidget {
    Q_OBJECT

public:
    explicit PatchWidget(ApiClient *apiClient, QWidget *parent = nullptr);

    void setGame(const QString &gameSlug, const QString &installPath,
                 const QList<GameConfig> &games);

signals:
    void backRequested();

private slots:
    void onManifestIndexReady(const QString &gameSlug, const ManifestIndex &index);
    void onTargetVersionChanged(int index);
    void onManifestReady(const QString &gameSlug, const Manifest &manifest);
    void onStartPatching();
    void onPatchProgress(const PatchProgress &progress);
    void onFileCompleted(const QString &path);
    void onFileFailed(const QString &path, const QString &error);
    void onPatchFinished(const PatchSummary &summary);

private:
    ApiClient *m_apiClient;
    GameScanner m_scanner;
    Patcher *m_patcher;

    QString m_gameSlug;
    QString m_installPath;
    QList<GameConfig> m_games;
    ScanResult m_scanResult;
    QSet<QString> m_knownHashes;
    QSet<QString> m_knownPaths;

    // UI elements
    QPushButton *m_backBtn;
    QLabel *m_gameLabel;
    QLabel *m_versionLabel;
    QComboBox *m_targetCombo;
    QLabel *m_scanResultsLabel;
    QPushButton *m_patchBtn;
    QProgressBar *m_overallProgress;
    QProgressBar *m_fileProgress;
    QLabel *m_fileProgressLabel;
    QTextEdit *m_log;
};
