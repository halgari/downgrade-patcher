#include <QApplication>
#include "api/ApiClient.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Downgrade Patcher");
    app.setApplicationVersion("0.1.0");

    QUrl serverUrl("https://downgradepatcher.wabbajack.org");
    ApiClient apiClient(serverUrl);

    MainWindow window(&apiClient);
    window.show();

    return app.exec();
}
