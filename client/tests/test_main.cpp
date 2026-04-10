#include <QCoreApplication>
#include <QTest>
#include "test_api_client.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestApiClient t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}
