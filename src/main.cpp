#include <QApplication>
#include <QIcon>

#include "app/MainWindow.hpp"
#include "core/Logger.hpp"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/app/icon.png"));
    QApplication::setApplicationName("CoD DS Level Studio");
    QApplication::setApplicationVersion("0.1.2");

    core::Logger::init();
    core::Logger::info("Starting CoD DS Level Studio");

    app::MainWindow window;
    window.resize(1280, 720);
    window.show();

    return app.exec();
}