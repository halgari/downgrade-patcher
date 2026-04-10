#pragma once
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "engine/SteamDetector.h"

class TestSteamDetector : public QObject {
    Q_OBJECT
private slots:
    void testParseLibraryFoldersVdf() {
        QString vdf = R"(
"libraryfolders"
{
    "0"
    {
        "path"        "C:\\Program Files (x86)\\Steam"
    }
    "1"
    {
        "path"        "D:\\SteamLibrary"
    }
}
)";
        auto folders = SteamDetector::parseSteamLibraryFolders(vdf);
        QCOMPARE(folders.size(), 2);
        QCOMPARE(folders[0], QString("C:/Program Files (x86)/Steam"));
        QCOMPARE(folders[1], QString("D:/SteamLibrary"));
    }

    void testParseEmptyVdf() {
        auto folders = SteamDetector::parseSteamLibraryFolders("");
        QCOMPARE(folders.size(), 0);
    }

    void testIsAppInstalled() {
        QTemporaryDir tmp;
        QString steamApps = tmp.path();
        QVERIFY(!SteamDetector::isAppInstalled(steamApps, 489830));

        QFile f(steamApps + "/appmanifest_489830.acf");
        f.open(QIODevice::WriteOnly);
        f.write("dummy");
        f.close();
        QVERIFY(SteamDetector::isAppInstalled(steamApps, 489830));
    }
};
