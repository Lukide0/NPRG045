
#include "App.h"
#include <QApplication>

int main(int argc, char* argv[]) {

    QApplication app(argc, argv);

    App main_window;
    main_window.show();

    return QApplication::exec();
}
