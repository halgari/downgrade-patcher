#include <QApplication>
#include <QLabel>
#include <QMainWindow>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Downgrade Patcher");
    window.resize(600, 400);

    auto *label = new QLabel("Downgrade Patcher - Loading...", &window);
    label->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(label);

    window.show();
    return app.exec();
}
