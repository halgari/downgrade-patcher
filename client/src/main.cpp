#include <QApplication>
#include <QFontDatabase>
#include "api/ApiClient.h"
#include "ui/MainWindow.h"

// Static plugin imports for static Qt builds
#ifdef Q_OS_WIN
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Downgrade Patcher");
    app.setApplicationVersion("0.1.0");

    // Load fonts embedded via Qt resource system (resources.qrc)
    QFontDatabase::addApplicationFont(":/fonts/CormorantGaramond-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/CormorantGaramond-Bold.ttf");
    QFontDatabase::addApplicationFont(":/fonts/CormorantGaramond-Medium.ttf");
    QFontDatabase::addApplicationFont(":/fonts/CormorantGaramond-Light.ttf");

    QUrl serverUrl("https://downgradepatcher.wabbajack.org");
    ApiClient apiClient(serverUrl);

    MainWindow window(&apiClient);
    window.show();

    return app.exec();
}
