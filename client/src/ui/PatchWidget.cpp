#include "ui/PatchWidget.h"
#include "engine/HashCache.h"
#include <QScrollBar>
#include <QFrame>
#include <QHeaderView>

PatchWidget::PatchWidget(ApiClient *apiClient, QWidget *parent)
    : QWidget(parent)
    , m_apiClient(apiClient)
    , m_patcher(new Patcher(apiClient, 4, this))
    , m_manifestsToLoad(0)
    , m_manifestsLoaded(0)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    // Header
    auto *header = new QHBoxLayout();
    m_backBtn = new QPushButton("\u2190 Back", this);
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
    auto *spacer = new QWidget(this);
    spacer->setFixedWidth(m_backBtn->sizeHint().width());
    header->addWidget(spacer);
    layout->addLayout(header);

    // Divider
    auto *divider = new QFrame(this);
    divider->setFixedHeight(2);
    divider->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "stop:0 transparent, stop:0.2 #6b4c1e, stop:0.5 #c4972a, "
        "stop:0.8 #6b4c1e, stop:1 transparent); border: none;"
    );
    layout->addWidget(divider);

    layout->addSpacing(4);

    // Version info
    m_versionLabel = new QLabel(this);
    m_versionLabel->setStyleSheet("color: #8a7a5a; font-size: 13px;");
    layout->addWidget(m_versionLabel);

    // Target selector
    auto *targetWidget = new QWidget(this);
    auto *targetLayout = new QHBoxLayout(targetWidget);
    targetLayout->setContentsMargins(0, 0, 0, 0);
    auto *targetLabel = new QLabel("Target version:", targetWidget);
    targetLabel->setStyleSheet("color: #b0a080; font-size: 13px; font-weight: bold;");
    targetLayout->addWidget(targetLabel);
    m_targetCombo = new QComboBox(targetWidget);
    m_targetCombo->setMinimumWidth(200);
    targetLayout->addWidget(m_targetCombo);
    targetLayout->addStretch();
    layout->addWidget(targetWidget);

    // Scan summary label
    m_scanSummaryLabel = new QLabel(this);
    m_scanSummaryLabel->setStyleSheet("color: #8a7a5a; font-size: 12px;");
    m_scanSummaryLabel->setVisible(false);
    layout->addWidget(m_scanSummaryLabel);

    // Scan results tree
    m_scanTree = new QTreeWidget(this);
    m_scanTree->setHeaderHidden(true);
    m_scanTree->setRootIsDecorated(true);
    m_scanTree->setVisible(false);
    m_scanTree->setMaximumHeight(200);
    m_scanTree->setStyleSheet(
        "QTreeWidget { background: #140e08; border: 2px solid #4a3520; border-radius: 4px; "
        "  font-size: 11px; padding: 4px; }"
        "QTreeWidget::item { padding: 2px 0; }"
        "QTreeWidget::branch { border: none; }"
    );
    layout->addWidget(m_scanTree);

    // Patch button
    m_patchBtn = new QPushButton("Start Patching", this);
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
    m_overallProgressLabel = new QLabel("Overall Progress", this);
    m_overallProgressLabel->setStyleSheet("color: #8a7a5a; font-size: 11px; font-weight: bold; letter-spacing: 1px;");
    m_overallProgressLabel->setVisible(false);
    layout->addWidget(m_overallProgressLabel);

    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setVisible(false);
    m_overallProgress->setTextVisible(true);
    m_overallProgress->setFormat("%v / %m files");
    layout->addWidget(m_overallProgress);

    m_fileProgressLabel = new QLabel(this);
    m_fileProgressLabel->setStyleSheet("color: #8a7a5a; font-size: 11px;");
    m_fileProgressLabel->setVisible(false);
    layout->addWidget(m_fileProgressLabel);

    m_fileProgress = new QProgressBar(this);
    m_fileProgress->setVisible(false);
    m_fileProgress->setTextVisible(true);
    m_fileProgress->setFormat("%v / %m");
    layout->addWidget(m_fileProgress);

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
    m_allManifests.clear();
    m_knownHashes.clear();
    m_knownPaths.clear();

    for (const auto &g : games) {
        if (g.slug == gameSlug) {
            m_gameLabel->setText(g.name);
            break;
        }
    }

    m_versionLabel->setText("Loading version data...");
    m_targetCombo->clear();
    m_scanSummaryLabel->setVisible(false);
    m_scanTree->setVisible(false);
    m_scanTree->clear();
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

    m_manifestIndex = index;

    // Detect installed version
    for (const auto &game : m_games) {
        if (game.slug != gameSlug) continue;
        QString exePath = m_installPath + "/" + game.exePath;
        QString exeHash = GameScanner::hashFile(exePath);
        if (index.versions.contains(exeHash)) {
            m_versionLabel->setText(
                QString("Current version: <b style='color: #c4972a;'>%1</b>")
                    .arg(index.versions[exeHash]));
        } else {
            m_versionLabel->setText(
                "Current version: <b style='color: #a05a2a;'>Unknown</b> "
                "<span style='color: #6a5a3a; font-size: 11px;'>(modified or unrecognized)</span>");
        }
        break;
    }

    // Fetch ALL manifests to build complete known hash set
    QSet<QString> versions;
    for (auto it = index.versions.begin(); it != index.versions.end(); ++it) {
        versions.insert(it.value());
    }

    m_manifestsToLoad = versions.size();
    m_manifestsLoaded = 0;
    m_versionLabel->setText(m_versionLabel->text() +
        QString(" <span style='color: #6a5a3a; font-size: 11px;'>(loading %1 manifests...)</span>")
            .arg(m_manifestsToLoad));

    // Connect the manifest-ready signal for bulk loading
    disconnect(m_apiClient, &ApiClient::manifestReady, this, nullptr);
    connect(m_apiClient, &ApiClient::manifestReady, this, &PatchWidget::onAllManifestReady);

    for (const auto &v : versions) {
        m_apiClient->fetchManifest(gameSlug, v);
    }
}

void PatchWidget::onAllManifestReady(const QString &gameSlug, const Manifest &manifest) {
    if (gameSlug != m_gameSlug) return;

    m_allManifests[manifest.version] = manifest;

    // Aggregate hashes and paths from this manifest
    for (const auto &f : manifest.files) {
        m_knownHashes.insert(f.xxhash3);
        m_knownPaths.insert(f.path);
    }

    ++m_manifestsLoaded;

    if (m_manifestsLoaded >= m_manifestsToLoad) {
        // All manifests loaded — populate the dropdown
        // Remove the "loading manifests" text
        for (const auto &game : m_games) {
            if (game.slug != m_gameSlug) continue;
            QString exePath = m_installPath + "/" + game.exePath;
            QString exeHash = GameScanner::hashFile(exePath);
            if (m_manifestIndex.versions.contains(exeHash)) {
                m_versionLabel->setText(
                    QString("Current version: <b style='color: #c4972a;'>%1</b>")
                        .arg(m_manifestIndex.versions[exeHash]));
            } else {
                m_versionLabel->setText(
                    "Current version: <b style='color: #a05a2a;'>Unknown</b>");
            }
            break;
        }

        QStringList versions = m_allManifests.keys();
        versions.sort();
        for (const auto &v : versions) {
            m_targetCombo->addItem(v);
        }
    }
}

void PatchWidget::onTargetVersionChanged(int index) {
    if (index < 0) return;
    if (m_manifestsLoaded < m_manifestsToLoad) return; // still loading

    QString version = m_targetCombo->currentText();
    if (!m_allManifests.contains(version)) return;

    m_scanSummaryLabel->setText("Scanning files...");
    m_scanSummaryLabel->setVisible(true);
    m_scanTree->setVisible(false);
    m_scanTree->clear();
    m_patchBtn->setVisible(false);

    runScan();
}

void PatchWidget::runScan() {
    QString version = m_targetCombo->currentText();
    const Manifest &manifest = m_allManifests[version];

    HashCache cache(m_installPath + "/.downgrade-patcher-cache.json");
    m_scanResult = m_scanner.scan(m_installPath, manifest, m_knownHashes, m_knownPaths, cache);

    int unchanged = m_scanResult.countByCategory(ScanCategory::Unchanged);
    int patchable = m_scanResult.countByCategory(ScanCategory::Patchable);
    int unknown = m_scanResult.countByCategory(ScanCategory::Unknown);
    int missing = m_scanResult.countByCategory(ScanCategory::Missing);
    int extra = m_scanResult.countByCategory(ScanCategory::Extra);
    int willPatch = patchable + missing;
    int total = m_scanResult.entries.size();

    m_scanSummaryLabel->setText(
        QString("Scan complete: %1 files checked").arg(total));
    m_scanSummaryLabel->setVisible(true);

    // Build tree — group patchable + missing together as "Will be patched"
    m_scanTree->clear();

    auto addCategory = [&](const QString &label, const QList<ScanCategory> &cats,
                           const QString &color, int count) {
        if (count == 0) return;
        auto *parent = new QTreeWidgetItem(m_scanTree);
        parent->setText(0, QString("%1 (%2)").arg(label).arg(count));
        parent->setForeground(0, QColor(color));
        parent->setExpanded(false);
        for (const auto &entry : m_scanResult.entries) {
            if (!cats.contains(entry.category)) continue;
            auto *child = new QTreeWidgetItem(parent);
            child->setText(0, entry.path);
            child->setForeground(0, QColor("#8a7a5a"));
        }
    };

    addCategory("Unchanged", {ScanCategory::Unchanged}, "#6a8a4a", unchanged);
    addCategory("Will be patched", {ScanCategory::Patchable, ScanCategory::Missing}, "#c4972a", willPatch);
    addCategory("Unknown (may fail)", {ScanCategory::Unknown}, "#a05a2a", unknown);
    addCategory("Will be removed", {ScanCategory::Extra}, "#8a4a3a", extra);

    m_scanTree->setVisible(true);
    m_patchBtn->setVisible(patchable > 0 || missing > 0 || extra > 0);
}

void PatchWidget::onStartPatching() {
    m_patchBtn->setVisible(false);
    m_backBtn->setEnabled(false);
    m_targetCombo->setEnabled(false);
    m_scanTree->setVisible(false);
    m_scanSummaryLabel->setVisible(false);
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
    m_fileProgressLabel->setText(progress.currentFile);
}

void PatchWidget::onFileCompleted(const QString &path) {
    m_log->append(QString("<span style='color: #6a8a4a;'>Patched:</span> <span style='color: #b0a080;'>%1</span>").arg(path));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void PatchWidget::onFileFailed(const QString &path, const QString &error) {
    m_log->append(QString("<span style='color: #a04a3a;'>Failed:</span> <span style='color: #8a6a4a;'>%1 — %2</span>").arg(path, error));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void PatchWidget::onPatchFinished(const PatchSummary &summary) {
    m_backBtn->setEnabled(true);
    m_fileProgress->setVisible(false);
    m_fileProgressLabel->setVisible(false);

    if (summary.failCount == 0) {
        m_log->append(QString(
            "<br><span style='color: #c4972a; font-size: 13px; font-weight: bold;'>"
            "Done! %1 files patched successfully.</span>")
            .arg(summary.successCount));
    } else {
        m_log->append(QString(
            "<br><span style='color: #a05a2a; font-size: 13px;'>"
            "Done: %1 succeeded, %2 failed.</span>")
            .arg(summary.successCount).arg(summary.failCount));
    }
}
