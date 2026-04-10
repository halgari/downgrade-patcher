#include <QApplication>
#include <QFontDatabase>
#include <QDir>
#include "api/ApiClient.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Downgrade Patcher");
    app.setApplicationVersion("0.1.0");

    // Load bundled fonts
    QString fontDir = QCoreApplication::applicationDirPath() + "/../resources/fonts";
    if (!QDir(fontDir).exists()) {
        // Try relative to source tree (dev mode)
        fontDir = QCoreApplication::applicationDirPath() + "/../../resources/fonts";
    }
    QFontDatabase::addApplicationFont(fontDir + "/CormorantGaramond-Regular.ttf");
    QFontDatabase::addApplicationFont(fontDir + "/CormorantGaramond-Bold.ttf");
    QFontDatabase::addApplicationFont(fontDir + "/CormorantGaramond-Medium.ttf");
    QFontDatabase::addApplicationFont(fontDir + "/CormorantGaramond-Light.ttf");

    QUrl serverUrl("https://downgradepatcher.wabbajack.org");
    ApiClient apiClient(serverUrl);

    MainWindow window(&apiClient);
    window.show();

    return app.exec();
}
