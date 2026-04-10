#include <QCoreApplication>
#include <QTest>
#include "test_api_client.h"
#include "test_hash_cache.h"
#include "test_steam_detector.h"
#include "test_game_scanner.h"
#include "test_patcher.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestApiClient t; status |= QTest::qExec(&t, argc, argv); }
    { TestHashCache t; status |= QTest::qExec(&t, argc, argv); }
    { TestSteamDetector t; status |= QTest::qExec(&t, argc, argv); }
    { TestGameScanner t; status |= QTest::qExec(&t, argc, argv); }
    { TestPatcher t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}
