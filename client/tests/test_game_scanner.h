#pragma once
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "engine/GameScanner.h"

class TestGameScanner : public QObject {
    Q_OBJECT
private slots:
    void testHashFile() {
        QTemporaryDir tmp;
        QString path = tmp.filePath("test.bin");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("hello world");
        f.close();

        auto hash = GameScanner::hashFile(path);
        QVERIFY(!hash.isEmpty());
        QCOMPARE(hash.length(), 16);
        QCOMPARE(GameScanner::hashFile(path), hash); // deterministic
    }

    void testHashFileDifferentContent() {
        QTemporaryDir tmp;
        QString p1 = tmp.filePath("a.bin");
        QString p2 = tmp.filePath("b.bin");
        QFile f1(p1); f1.open(QIODevice::WriteOnly); f1.write("hello"); f1.close();
        QFile f2(p2); f2.open(QIODevice::WriteOnly); f2.write("world"); f2.close();
        QVERIFY(GameScanner::hashFile(p1) != GameScanner::hashFile(p2));
    }

    void testScanCategorizesCorrectly() {
        QTemporaryDir tmp;
        QString installPath = tmp.path();

        QFile exe(installPath + "/game.exe");
        exe.open(QIODevice::WriteOnly); exe.write("exe-old"); exe.close();

        QDir().mkpath(installPath + "/Data");
        QFile esm(installPath + "/Data/main.esm");
        esm.open(QIODevice::WriteOnly); esm.write("esm-same"); esm.close();

        QString exeHash = GameScanner::hashFile(installPath + "/game.exe");
        QString esmHash = GameScanner::hashFile(installPath + "/Data/main.esm");

        Manifest target;
        target.game = "test";
        target.version = "2.0";
        target.files.append({"game.exe", 7, "target-exe-hash"});
        target.files.append({"Data/main.esm", 8, esmHash});
        target.files.append({"Data/new.bsa", 1000, "new-bsa-hash"});

        QSet<QString> knownHashes = {exeHash, esmHash, "target-exe-hash", "new-bsa-hash"};
        QSet<QString> knownPaths = {"game.exe", "Data/main.esm", "Data/new.bsa"};

        HashCache cache(tmp.filePath("cache.json"));
        GameScanner scanner;
        auto result = scanner.scan(installPath, target, knownHashes, knownPaths, cache);

        QCOMPARE(result.countByCategory(ScanCategory::Unchanged), 1);
        QCOMPARE(result.countByCategory(ScanCategory::Patchable), 1);
        QCOMPARE(result.countByCategory(ScanCategory::Missing), 1);
    }

    void testScanDetectsExtraFiles() {
        QTemporaryDir tmp;
        QString installPath = tmp.path();

        QFile exe(installPath + "/game.exe");
        exe.open(QIODevice::WriteOnly); exe.write("exe"); exe.close();
        QFile extra(installPath + "/old.dll");
        extra.open(QIODevice::WriteOnly); extra.write("old"); extra.close();

        QString exeHash = GameScanner::hashFile(installPath + "/game.exe");

        Manifest target;
        target.game = "test";
        target.version = "2.0";
        target.files.append({"game.exe", 3, exeHash});

        QSet<QString> knownHashes = {exeHash};
        QSet<QString> knownPaths = {"game.exe", "old.dll"};

        HashCache cache(tmp.filePath("cache.json"));
        GameScanner scanner;
        auto result = scanner.scan(installPath, target, knownHashes, knownPaths, cache);

        QCOMPARE(result.countByCategory(ScanCategory::Unchanged), 1);
        QCOMPARE(result.countByCategory(ScanCategory::Extra), 1);
    }

    void testScanUnknownHash() {
        QTemporaryDir tmp;
        QString installPath = tmp.path();

        QFile exe(installPath + "/game.exe");
        exe.open(QIODevice::WriteOnly); exe.write("modded-exe"); exe.close();

        Manifest target;
        target.game = "test";
        target.version = "2.0";
        target.files.append({"game.exe", 10, "target-hash"});

        QSet<QString> knownHashes = {"target-hash"};
        QSet<QString> knownPaths = {"game.exe"};

        HashCache cache(tmp.filePath("cache.json"));
        GameScanner scanner;
        auto result = scanner.scan(installPath, target, knownHashes, knownPaths, cache);

        QCOMPARE(result.countByCategory(ScanCategory::Unknown), 1);
    }
};
