#include <QCoreApplication>
#include <QTest>
#include "test_api_client.h"
#include "test_hash_cache.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestApiClient t; status |= QTest::qExec(&t, argc, argv); }
    { TestHashCache t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}
