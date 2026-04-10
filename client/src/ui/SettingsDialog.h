#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    bool bestOfBothWorlds() const;
    QString manualGamePath() const;

private:
    QCheckBox *m_botbwCheck;
    QLineEdit *m_manualPath;

    void loadSettings();
    void saveSettings();
};
