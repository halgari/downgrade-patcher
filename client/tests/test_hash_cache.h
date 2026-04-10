#pragma once
#include <QTest>
#include <QTemporaryDir>
#include "engine/HashCache.h"

class TestHashCache : public QObject {
    Q_OBJECT
private slots:
    void testLookupMiss() {
        QTemporaryDir tmp;
        HashCache cache(tmp.filePath("cache.json"));
        auto result = cache.lookup("SkyrimSE.exe", 1000, QDateTime::currentDateTime());
        QVERIFY(result.isEmpty());
    }

    void testStoreAndLookup() {
        QTemporaryDir tmp;
        HashCache cache(tmp.filePath("cache.json"));
        QDateTime mtime = QDateTime::fromMSecsSinceEpoch(1700000000000);
        cache.store("SkyrimSE.exe", 1000, mtime, "abc123");
        QCOMPARE(cache.lookup("SkyrimSE.exe", 1000, mtime), QString("abc123"));
    }

    void testLookupInvalidatedBySize() {
        QTemporaryDir tmp;
        HashCache cache(tmp.filePath("cache.json"));
        QDateTime mtime = QDateTime::fromMSecsSinceEpoch(1700000000000);
        cache.store("SkyrimSE.exe", 1000, mtime, "abc123");
        QVERIFY(cache.lookup("SkyrimSE.exe", 2000, mtime).isEmpty());
    }

    void testLookupInvalidatedByMtime() {
        QTemporaryDir tmp;
        HashCache cache(tmp.filePath("cache.json"));
        QDateTime mtime1 = QDateTime::fromMSecsSinceEpoch(1700000000000);
        QDateTime mtime2 = QDateTime::fromMSecsSinceEpoch(1700000001000);
        cache.store("SkyrimSE.exe", 1000, mtime1, "abc123");
        QVERIFY(cache.lookup("SkyrimSE.exe", 1000, mtime2).isEmpty());
    }

    void testSaveAndLoad() {
        QTemporaryDir tmp;
        QString path = tmp.filePath("cache.json");
        QDateTime mtime = QDateTime::fromMSecsSinceEpoch(1700000000000);
        {
            HashCache cache(path);
            cache.store("SkyrimSE.exe", 1000, mtime, "abc123");
            cache.store("Data/Skyrim.esm", 5000, mtime, "def456");
            cache.save();
        }
        {
            HashCache cache(path);
            cache.load();
            QCOMPARE(cache.lookup("SkyrimSE.exe", 1000, mtime), QString("abc123"));
            QCOMPARE(cache.lookup("Data/Skyrim.esm", 5000, mtime), QString("def456"));
        }
    }
};
