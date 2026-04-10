#pragma once
#include <QTest>
#include <zstd.h>
#include "engine/Patcher.h"

class TestPatcher : public QObject {
    Q_OBJECT
private slots:
    void testFindClosestDonor() {
        QStringList locals = {"Data/Skyrim.esm", "Data/Update.esm", "SkyrimSE.exe"};
        QCOMPARE(Patcher::findClosestDonor("Data/Skyrim.esm", locals), QString("Data/Skyrim.esm"));
    }

    void testFindClosestDonorByFilename() {
        QStringList locals = {"OldDir/textures.bsa", "other.dll"};
        QCOMPARE(Patcher::findClosestDonor("NewDir/textures.bsa", locals), QString("OldDir/textures.bsa"));
    }

    void testFindClosestDonorNoMatch() {
        QStringList locals = {"game.exe", "data.bsa"};
        QCOMPARE(Patcher::findClosestDonor("completely_new.dll", locals), QString());
    }

    void testDecompressChunkWithoutDict() {
        QByteArray original = "Hello, this is test data for compression!";
        size_t bound = ZSTD_compressBound(original.size());
        QByteArray compressed(bound, Qt::Uninitialized);
        size_t compSize = ZSTD_compress(compressed.data(), compressed.size(),
                                         original.constData(), original.size(), 1);
        QVERIFY(!ZSTD_isError(compSize));
        compressed.resize(compSize);

        QByteArray decompressed = Patcher::decompressChunk(compressed, {});
        QCOMPARE(decompressed, original);
    }

    void testPatchSummary() {
        PatchSummary summary{5, 2, {"a.exe", "b.dll"}};
        QCOMPARE(summary.successCount, 5);
        QCOMPARE(summary.failCount, 2);
        QCOMPARE(summary.failedFiles.size(), 2);
    }
};
