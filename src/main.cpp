// main.cpp - entry point
#include <QApplication>
#include "editor/ui/window_main.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    ollygon::MainWindow window;
    window.show();

    return app.exec();
}
