#include "ui/PatchWidget.h"
#include "engine/HashCache.h"
#include <QScrollBar>

PatchWidget::PatchWidget(ApiClient *apiClient, QWidget *parent)
    : QWidget(parent)
    , m_apiClient(apiClient)
    , m_patcher(new Patcher(apiClient, 4, this))
{
    auto *layout = new QVBoxLayout(this);

    // Header
    auto *header = new QHBoxLayout();
    m_backBtn = new QPushButton("← Back", this);
    m_gameLabel = new QLabel(this);
    m_gameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    header->addWidget(m_backBtn);
    header->addWidget(m_gameLabel, 1);
    layout->addLayout(header);

    // Version info
    m_versionLabel = new QLabel(this);
    m_versionLabel->setStyleSheet("color: #aaaaaa;");
    layout->addWidget(m_versionLabel);

    // Target selector
    auto *targetLayout = new QHBoxLayout();
    targetLayout->addWidget(new QLabel("Target version:", this));
    m_targetCombo = new QComboBox(this);
    m_targetCombo->setMinimumWidth(200);
    targetLayout->addWidget(m_targetCombo);
    targetLayout->addStretch();
    layout->addLayout(targetLayout);

    // Scan results
    m_scanResultsLabel = new QLabel(this);
    m_scanResultsLabel->setStyleSheet(
        "background: #2a2a3e; border: 1px solid #444; border-radius: 4px; padding: 8px;"
    );
    m_scanResultsLabel->setVisible(false);
    layout->addWidget(m_scanResultsLabel);

    // Patch button
    m_patchBtn = new QPushButton("Start Patching", this);
    m_patchBtn->setStyleSheet("padding: 10px; font-size: 14px;");
    m_patchBtn->setVisible(false);
    layout->addWidget(m_patchBtn);

    // Progress bars
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setVisible(false);
    layout->addWidget(m_overallProgress);

    m_fileProgressLabel = new QLabel(this);
    m_fileProgressLabel->setVisible(false);
    layout->addWidget(m_fileProgressLabel);

    m_fileProgress = new QProgressBar(this);
    m_fileProgress->setVisible(false);
    layout->addWidget(m_fileProgress);

    // Log
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setVisible(false);
    m_log->setMaximumHeight(150);
    m_log->setStyleSheet("background: #1e1e2e; border: 1px solid #333; font-size: 11px;");
    layout->addWidget(m_log);

    layout->addStretch();

    // Connections
    connect(m_backBtn, &QPushButton::clicked, this, &PatchWidget::backRequested);
    connect(m_targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PatchWidget::onTargetVersionChanged);
    connect(m_apiClient, &ApiClient::manifestIndexReady, this, &PatchWidget::onManifestIndexReady);
    connect(m_apiClient, &ApiClient::manifestReady, this, &PatchWidget::onManifestReady);
    connect(m_patchBtn, &QPushButton::clicked, this, &PatchWidget::onStartPatching);
    connect(m_patcher, &Patcher::progressUpdated, this, &PatchWidget::onPatchProgress);
    connect(m_patcher, &Patcher::fileCompleted, this, &PatchWidget::onFileCompleted);
    connect(m_patcher, &Patcher::fileFailed, this, &PatchWidget::onFileFailed);
    connect(m_patcher, &Patcher::finished, this, &PatchWidget::onPatchFinished);
}

void PatchWidget::setGame(const QString &gameSlug, const QString &installPath,
                          const QList<GameConfig> &games) {
    m_gameSlug = gameSlug;
    m_installPath = installPath;
    m_games = games;

    // Find game name
    for (const auto &g : games) {
        if (g.slug == gameSlug) {
            m_gameLabel->setText(g.name);
            break;
        }
    }

    // Detect current version
    m_versionLabel->setText("Detecting version...");
    m_targetCombo->clear();
    m_scanResultsLabel->setVisible(false);
    m_patchBtn->setVisible(false);
    m_overallProgress->setVisible(false);
    m_fileProgress->setVisible(false);
    m_fileProgressLabel->setVisible(false);
    m_log->setVisible(false);
    m_log->clear();

    m_apiClient->fetchManifestIndex(gameSlug);
}

void PatchWidget::onManifestIndexReady(const QString &gameSlug, const ManifestIndex &index) {
    if (gameSlug != m_gameSlug) return;

    // Detect installed version by hashing the exe
    for (const auto &game : m_games) {
        if (game.slug != gameSlug) continue;
        QString exePath = m_installPath + "/" + game.exePath;
        QString exeHash = GameScanner::hashFile(exePath);
        if (index.versions.contains(exeHash)) {
            m_versionLabel->setText(QString("Current version: %1").arg(index.versions[exeHash]));
        } else {
            m_versionLabel->setText(QString("Current version: unknown"));
        }
        break;
    }

    // Populate target versions
    QSet<QString> versions;
    for (auto it = index.versions.begin(); it != index.versions.end(); ++it) {
        versions.insert(it.value());
    }
    for (const auto &v : versions) {
        m_targetCombo->addItem(v);
    }
}

void PatchWidget::onTargetVersionChanged(int index) {
    if (index < 0) return;
    QString version = m_targetCombo->currentText();
    m_scanResultsLabel->setText("Loading manifest...");
    m_scanResultsLabel->setVisible(true);
    m_patchBtn->setVisible(false);
    m_apiClient->fetchManifest(m_gameSlug, version);
}

void PatchWidget::onManifestReady(const QString &gameSlug, const Manifest &manifest) {
    if (gameSlug != m_gameSlug) return;

    // Build known hashes and paths from this manifest
    // In a full implementation, we'd aggregate across all versions
    m_knownHashes.clear();
    m_knownPaths.clear();
    for (const auto &f : manifest.files) {
        m_knownHashes.insert(f.xxhash3);
        m_knownPaths.insert(f.path);
    }

    // Scan
    HashCache cache(m_installPath + "/.downgrade-patcher-cache.json");
    m_scanResult = m_scanner.scan(m_installPath, manifest, m_knownHashes, m_knownPaths, cache);

    // Display results
    QString text;
    int unchanged = m_scanResult.countByCategory(ScanCategory::Unchanged);
    int patchable = m_scanResult.countByCategory(ScanCategory::Patchable);
    int unknown = m_scanResult.countByCategory(ScanCategory::Unknown);
    int missing = m_scanResult.countByCategory(ScanCategory::Missing);
    int extra = m_scanResult.countByCategory(ScanCategory::Extra);

    text += QString("✓ %1 files unchanged\n").arg(unchanged);
    if (patchable > 0) text += QString("↻ %1 files will be patched\n").arg(patchable);
    if (unknown > 0) text += QString("⚠ %1 files unknown (may fail)\n").arg(unknown);
    if (missing > 0) text += QString("+ %1 new files to download\n").arg(missing);
    if (extra > 0) text += QString("✕ %1 files will be removed\n").arg(extra);

    m_scanResultsLabel->setText(text.trimmed());
    m_patchBtn->setVisible(patchable > 0 || missing > 0 || extra > 0);
}

void PatchWidget::onStartPatching() {
    m_patchBtn->setVisible(false);
    m_backBtn->setEnabled(false);
    m_targetCombo->setEnabled(false);
    m_overallProgress->setVisible(true);
    m_overallProgress->setValue(0);
    m_fileProgress->setVisible(true);
    m_fileProgress->setValue(0);
    m_fileProgressLabel->setVisible(true);
    m_log->setVisible(true);

    m_patcher->start(m_gameSlug, m_installPath, m_scanResult);
}

void PatchWidget::onPatchProgress(const PatchProgress &progress) {
    m_overallProgress->setMaximum(progress.totalFiles);
    m_overallProgress->setValue(progress.filesCompleted);
    m_fileProgress->setMaximum(progress.currentFileTotalChunks);
    m_fileProgress->setValue(progress.currentFileChunksCompleted);
    m_fileProgressLabel->setText(
        QString("%1 — chunk %2/%3")
            .arg(progress.currentFile)
            .arg(progress.currentFileChunksCompleted)
            .arg(progress.currentFileTotalChunks)
    );
}

void PatchWidget::onFileCompleted(const QString &path) {
    m_log->append(QString("<span style='color: #4a9;'>✓ %1 — patched</span>").arg(path));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void PatchWidget::onFileFailed(const QString &path, const QString &error) {
    m_log->append(QString("<span style='color: #e55;'>✕ %1 — %2</span>").arg(path, error));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void PatchWidget::onPatchFinished(const PatchSummary &summary) {
    m_backBtn->setEnabled(true);
    m_fileProgress->setVisible(false);
    m_fileProgressLabel->setVisible(false);

    QString msg = QString("\n— Done: %1 succeeded, %2 failed —")
        .arg(summary.successCount).arg(summary.failCount);
    m_log->append(msg);
}
