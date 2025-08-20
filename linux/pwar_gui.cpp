#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "libpwar.h"
#include "PwarController.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    PwarController controller;
    engine.rootContext()->setContextProperty("pwarController", &controller);
    engine.load(QUrl::fromLocalFile("./pwar_gui.qml"));
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
