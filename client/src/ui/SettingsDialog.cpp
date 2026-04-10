#include "ui/SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);

    // Best of both worlds
    m_botbwCheck = new QCheckBox("Enable Best-of-Both-Worlds mode", this);
    m_botbwCheck->setToolTip(
        "When enabled, you can select different versions for program files "
        "(exe, DLLs) and data files (BSAs, ESPs, ESMs) independently."
    );
    layout->addWidget(m_botbwCheck);

    // Manual game path
    layout->addSpacing(12);
    layout->addWidget(new QLabel("Manual game directory:", this));
    auto *pathLayout = new QHBoxLayout();
    m_manualPath = new QLineEdit(this);
    m_manualPath->setPlaceholderText("Leave empty for auto-detection");
    auto *browseBtn = new QPushButton("Browse...", this);
    pathLayout->addWidget(m_manualPath, 1);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        auto dir = QFileDialog::getExistingDirectory(this, "Select game directory");
        if (!dir.isEmpty()) m_manualPath->setText(dir);
    });

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadSettings();
}

bool SettingsDialog::bestOfBothWorlds() const {
    return m_botbwCheck->isChecked();
}

QString SettingsDialog::manualGamePath() const {
    return m_manualPath->text();
}

void SettingsDialog::loadSettings() {
    QSettings settings;
    m_botbwCheck->setChecked(settings.value("bestOfBothWorlds", false).toBool());
    m_manualPath->setText(settings.value("manualGamePath", "").toString());
}

void SettingsDialog::saveSettings() {
    QSettings settings;
    settings.setValue("bestOfBothWorlds", m_botbwCheck->isChecked());
    settings.setValue("manualGamePath", m_manualPath->text());
}
