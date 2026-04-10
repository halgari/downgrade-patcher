#include "engine/SteamDetector.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

SteamDetector::SteamDetector(QObject *parent) : QObject(parent) {}

QStringList SteamDetector::defaultSteamPaths() const {
    QStringList paths;
#ifdef Q_OS_WIN
    paths << "C:/Program Files (x86)/Steam";
    // Check Windows registry
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam",
                  QSettings::NativeFormat);
    QString regPath = reg.value("InstallPath").toString();
    if (!regPath.isEmpty()) paths << regPath;
#else
    QString home = QDir::homePath();
    paths << home + "/.steam/steam";
    paths << home + "/.local/share/Steam";
#endif
    return paths;
}

QString SteamDetector::findSteamRoot() const {
    for (const auto &path : defaultSteamPaths()) {
        if (QDir(path).exists()) return path;
    }
    return {};
}

QStringList SteamDetector::parseSteamLibraryFolders(const QString &vdfContent) {
    QStringList folders;
    // Parse Valve VDF format: look for "path" keys
    QRegularExpression re(R"re("path"\s+"([^"]+)")re");
    auto it = re.globalMatch(vdfContent);
    while (it.hasNext()) {
        auto match = it.next();
        folders << match.captured(1).replace("\\\\", "/");
    }
    return folders;
}

QStringList SteamDetector::findSteamLibraryFolders() const {
    QStringList folders;
    QString steamRoot = findSteamRoot();
    if (steamRoot.isEmpty()) return folders;

    // The Steam root itself is always a library folder
    folders << steamRoot;

    // Parse libraryfolders.vdf for additional library paths
    QString vdfPath = steamRoot + "/steamapps/libraryfolders.vdf";
    QFile vdf(vdfPath);
    if (vdf.open(QIODevice::ReadOnly)) {
        auto parsed = parseSteamLibraryFolders(QString::fromUtf8(vdf.readAll()));
        for (const auto &folder : parsed) {
            if (!folders.contains(folder)) folders << folder;
        }
    }

    return folders;
}

bool SteamDetector::isAppInstalled(const QString &steamAppsDir, int appId) {
    return QFile::exists(
        QString("%1/appmanifest_%2.acf").arg(steamAppsDir, QString::number(appId))
    );
}

QList<DetectedGame> SteamDetector::detectGames(const QList<GameConfig> &knownGames) const {
    QList<DetectedGame> detected;
    auto folders = findSteamLibraryFolders();

    for (const auto &folder : folders) {
        QString steamApps = folder + "/steamapps";
        if (!QDir(steamApps).exists()) continue;

        for (const auto &game : knownGames) {
            if (isAppInstalled(steamApps, game.steamAppId)) {
                // Read appmanifest to find install dir
                QString manifestPath = QString("%1/appmanifest_%2.acf")
                    .arg(steamApps, QString::number(game.steamAppId));
                QFile manifest(manifestPath);
                if (!manifest.open(QIODevice::ReadOnly)) continue;

                QString content = QString::fromUtf8(manifest.readAll());
                QRegularExpression re(R"re("installdir"\s+"([^"]+)")re");
                auto match = re.match(content);
                if (match.hasMatch()) {
                    QString installDir = steamApps + "/common/" + match.captured(1);
                    if (QDir(installDir).exists()) {
                        detected.append(DetectedGame{game.slug, installDir});
                    }
                }
            }
        }
    }

    return detected;
}
