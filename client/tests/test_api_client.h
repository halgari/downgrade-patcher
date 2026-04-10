#pragma once
#include <QTest>
#include "api/types.h"

class TestApiClient : public QObject {
    Q_OBJECT
private slots:
    void testGameConfigStruct() {
        GameConfig config;
        config.slug = "skyrim-se";
        config.steamAppId = 489830;
        QCOMPARE(config.slug, QString("skyrim-se"));
        QCOMPARE(config.steamAppId, 489830);
    }

    void testManifestStruct() {
        Manifest m;
        m.game = "skyrim-se";
        m.version = "1.5.97";
        FileEntry f;
        f.path = "SkyrimSE.exe";
        f.size = 75210240;
        f.xxhash3 = "abc123";
        m.files.append(f);
        QCOMPARE(m.files.size(), 1);
        QCOMPARE(m.files[0].path, QString("SkyrimSE.exe"));
        QCOMPARE(m.files[0].size, qint64(75210240));
    }

    void testScanResultCountByCategory() {
        ScanResult result;
        result.entries.append({"a.exe", ScanCategory::Unchanged, "h1", "h1"});
        result.entries.append({"b.bsa", ScanCategory::Patchable, "h2", "h3"});
        result.entries.append({"c.dll", ScanCategory::Patchable, "h4", "h5"});
        result.entries.append({"d.esm", ScanCategory::Unknown, "h6", "h7"});
        QCOMPARE(result.countByCategory(ScanCategory::Unchanged), 1);
        QCOMPARE(result.countByCategory(ScanCategory::Patchable), 2);
        QCOMPARE(result.countByCategory(ScanCategory::Unknown), 1);
        QCOMPARE(result.countByCategory(ScanCategory::Missing), 0);
    }

    void testPatchMeta() {
        PatchMeta meta;
        meta.totalChunks = 3;
        meta.chunks.append({0, 8000});
        meta.chunks.append({1, 8000});
        meta.chunks.append({2, 3500});
        QCOMPARE(meta.totalChunks, 3);
        QCOMPARE(meta.chunks[2].size, qint64(3500));
    }
};
