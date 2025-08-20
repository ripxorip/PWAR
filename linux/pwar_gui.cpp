#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "libpwar.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile("./pwar_gui.qml"));
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
