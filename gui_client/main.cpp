#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("WeatherGUI");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("WeatherPort");

    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    MainWindow window;
    window.show();

    return app.exec();
}
