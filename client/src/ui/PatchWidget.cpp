#include "ui/PatchWidget.h"
#include "engine/HashCache.h"
#include <QScrollBar>
#include <QFrame>

PatchWidget::PatchWidget(ApiClient *apiClient, QWidget *parent)
    : QWidget(parent)
    , m_apiClient(apiClient)
    , m_patcher(new Patcher(apiClient, 4, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(10);

    // Header
    auto *header = new QHBoxLayout();
    m_backBtn = new QPushButton("\u2190 Return", this);
    m_backBtn->setStyleSheet(
        "border: 1px solid #5c3d2e; background: transparent; "
        "color: #8a7a5a; padding: 6px 14px; font-size: 11px;"
    );
    m_gameLabel = new QLabel(this);
    m_gameLabel->setStyleSheet(
        "font-weight: bold; font-size: 22px; color: #c4972a; letter-spacing: 2px;"
    );
    m_gameLabel->setAlignment(Qt::AlignCenter);
    header->addWidget(m_backBtn);
    header->addWidget(m_gameLabel, 1);
    // Spacer to center the title
    auto *spacer = new QWidget(this);
    spacer->setFixedWidth(m_backBtn->sizeHint().width());
    header->addWidget(spacer);
    layout->addLayout(header);

    // Decorative divider
    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 transparent, stop:0.2 #6b4c1e, stop:0.5 #c4972a, "
        "stop:0.8 #6b4c1e, stop:1 transparent); "
        "border: none; max-height: 2px; margin: 4px 40px;"
    );
    layout->addWidget(divider);

    // Version info section
    auto *versionSection = new QWidget(this);
    auto *versionLayout = new QVBoxLayout(versionSection);
    versionLayout->setContentsMargins(0, 8, 0, 8);
    versionLayout->setSpacing(12);

    m_versionLabel = new QLabel(this);
    m_versionLabel->setStyleSheet("color: #8a7a5a; font-size: 13px;");
    versionLayout->addWidget(m_versionLabel);

    // Target selector
    auto *targetWidget = new QWidget(versionSection);
    auto *targetLayout = new QHBoxLayout(targetWidget);
    targetLayout->setContentsMargins(0, 0, 0, 0);
    auto *targetLabel = new QLabel("Restore to:", targetWidget);
    targetLabel->setStyleSheet("color: #b0a080; font-size: 13px; font-weight: bold;");
    targetLayout->addWidget(targetLabel);
    m_targetCombo = new QComboBox(targetWidget);
    m_targetCombo->setMinimumWidth(200);
    targetLayout->addWidget(m_targetCombo);
    targetLayout->addStretch();
    versionLayout->addWidget(targetWidget);

    layout->addWidget(versionSection);

    // Scan results panel
    m_scanResultsLabel = new QLabel(this);
    m_scanResultsLabel->setStyleSheet(
        "background: #1a1208; border: 2px solid #4a3520; border-radius: 6px; "
        "padding: 12px; font-size: 12px; line-height: 1.6;"
    );
    m_scanResultsLabel->setVisible(false);
    m_scanResultsLabel->setWordWrap(true);
    layout->addWidget(m_scanResultsLabel);

    // Patch button
    m_patchBtn = new QPushButton("Begin Restoration", this);
    m_patchBtn->setStyleSheet(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "    stop:0 #6b4c1e, stop:1 #4a3010); "
        "  border: 2px solid #c4972a; border-radius: 4px; "
        "  padding: 12px; font-size: 15px; font-weight: bold; "
        "  color: #e8d5a3; letter-spacing: 1px; "
        "} "
        "QPushButton:hover { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "    stop:0 #8b6914, stop:1 #6b4c1e); "
        "  border-color: #e8b84a; "
        "}"
    );
    m_patchBtn->setVisible(false);
    layout->addWidget(m_patchBtn);

    // Progress section
    auto *progressSection = new QWidget(this);
    auto *progressLayout = new QVBoxLayout(progressSection);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(6);

    auto *overallLabel = new QLabel("Overall Progress", progressSection);
    overallLabel->setStyleSheet("color: #8a7a5a; font-size: 11px; font-weight: bold; letter-spacing: 1px;");
    overallLabel->setVisible(false);
    progressLayout->addWidget(overallLabel);
    m_overallProgressLabel = overallLabel;

    m_overallProgress = new QProgressBar(progressSection);
    m_overallProgress->setVisible(false);
    m_overallProgress->setTextVisible(true);
    m_overallProgress->setFormat("%v / %m files");
    progressLayout->addWidget(m_overallProgress);

    m_fileProgressLabel = new QLabel(progressSection);
    m_fileProgressLabel->setStyleSheet("color: #8a7a5a; font-size: 11px;");
    m_fileProgressLabel->setVisible(false);
    progressLayout->addWidget(m_fileProgressLabel);

    m_fileProgress = new QProgressBar(progressSection);
    m_fileProgress->setVisible(false);
    m_fileProgress->setTextVisible(true);
    m_fileProgress->setFormat("chunk %v / %m");
    progressLayout->addWidget(m_fileProgress);

    layout->addWidget(progressSection);

    // Log
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setVisible(false);
    m_log->setMaximumHeight(180);
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

    for (const auto &g : games) {
        if (g.slug == gameSlug) {
            m_gameLabel->setText(g.name);
            break;
        }
    }

    m_versionLabel->setText("Consulting the archives...");
    m_targetCombo->clear();
    m_scanResultsLabel->setVisible(false);
    m_patchBtn->setVisible(false);
    m_overallProgress->setVisible(false);
    m_overallProgressLabel->setVisible(false);
    m_fileProgress->setVisible(false);
    m_fileProgressLabel->setVisible(false);
    m_log->setVisible(false);
    m_log->clear();

    m_apiClient->fetchManifestIndex(gameSlug);
}

void PatchWidget::onManifestIndexReady(const QString &gameSlug, const ManifestIndex &index) {
    if (gameSlug != m_gameSlug) return;

    for (const auto &game : m_games) {
        if (game.slug != gameSlug) continue;
        QString exePath = m_installPath + "/" + game.exePath;
        QString exeHash = GameScanner::hashFile(exePath);
        if (index.versions.contains(exeHash)) {
            m_versionLabel->setText(
                QString("Current inscription: <b style='color: #c4972a;'>%1</b>")
                    .arg(index.versions[exeHash]));
        } else {
            m_versionLabel->setText(
                "Current inscription: <b style='color: #a05a2a;'>Unknown</b> "
                "<span style='color: #6a5a3a; font-size: 11px;'>(modified or unrecognized)</span>");
        }
        break;
    }

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
    m_scanResultsLabel->setText(
        "<span style='color: #8a7a5a; font-style: italic;'>Scanning scrolls...</span>");
    m_scanResultsLabel->setVisible(true);
    m_patchBtn->setVisible(false);
    m_apiClient->fetchManifest(m_gameSlug, version);
}

void PatchWidget::onManifestReady(const QString &gameSlug, const Manifest &manifest) {
    if (gameSlug != m_gameSlug) return;

    m_knownHashes.clear();
    m_knownPaths.clear();
    for (const auto &f : manifest.files) {
        m_knownHashes.insert(f.xxhash3);
        m_knownPaths.insert(f.path);
    }

    HashCache cache(m_installPath + "/.downgrade-patcher-cache.json");
    m_scanResult = m_scanner.scan(m_installPath, manifest, m_knownHashes, m_knownPaths, cache);

    int unchanged = m_scanResult.countByCategory(ScanCategory::Unchanged);
    int patchable = m_scanResult.countByCategory(ScanCategory::Patchable);
    int unknown = m_scanResult.countByCategory(ScanCategory::Unknown);
    int missing = m_scanResult.countByCategory(ScanCategory::Missing);
    int extra = m_scanResult.countByCategory(ScanCategory::Extra);

    QString text = "<table cellspacing='6'>";
    text += QString("<tr><td style='color: #6a8a4a;'>%1</td><td style='color: #8a7a5a;'>files unchanged</td></tr>").arg(unchanged);
    if (patchable > 0)
        text += QString("<tr><td style='color: #c4972a;'>%1</td><td style='color: #b0a080;'>files will be restored</td></tr>").arg(patchable);
    if (unknown > 0)
        text += QString("<tr><td style='color: #a05a2a;'>%1</td><td style='color: #8a6a4a;'>files unrecognized (may fail)</td></tr>").arg(unknown);
    if (missing > 0)
        text += QString("<tr><td style='color: #7a8ab0;'>%1</td><td style='color: #8a7a5a;'>new files to inscribe</td></tr>").arg(missing);
    if (extra > 0)
        text += QString("<tr><td style='color: #8a4a3a;'>%1</td><td style='color: #8a7a5a;'>files to erase</td></tr>").arg(extra);
    text += "</table>";

    m_scanResultsLabel->setText(text);
    m_patchBtn->setVisible(patchable > 0 || missing > 0 || extra > 0);
}

void PatchWidget::onStartPatching() {
    m_patchBtn->setVisible(false);
    m_backBtn->setEnabled(false);
    m_targetCombo->setEnabled(false);
    m_overallProgress->setVisible(true);
    m_overallProgressLabel->setVisible(true);
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
        QString("<span style='color: #8a7a5a;'>%1</span>")
            .arg(progress.currentFile));
}

void PatchWidget::onFileCompleted(const QString &path) {
    m_log->append(QString("<span style='color: #6a8a4a;'>Restored:</span> <span style='color: #b0a080;'>%1</span>").arg(path));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void PatchWidget::onFileFailed(const QString &path, const QString &error) {
    m_log->append(QString("<span style='color: #a04a3a;'>Failed:</span> <span style='color: #8a6a4a;'>%1 &mdash; %2</span>").arg(path, error));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void PatchWidget::onPatchFinished(const PatchSummary &summary) {
    m_backBtn->setEnabled(true);
    m_fileProgress->setVisible(false);
    m_fileProgressLabel->setVisible(false);

    if (summary.failCount == 0) {
        m_log->append(QString(
            "<br><span style='color: #c4972a; font-size: 13px; font-weight: bold;'>"
            "Restoration complete! %1 files restored.</span>")
            .arg(summary.successCount));
    } else {
        m_log->append(QString(
            "<br><span style='color: #a05a2a; font-size: 13px;'>"
            "Restoration finished: %1 succeeded, %2 failed.</span>")
            .arg(summary.successCount).arg(summary.failCount));
    }
}
