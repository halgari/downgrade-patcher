#pragma once
#include <QTest>

class TestMain : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(true);
    }
};
