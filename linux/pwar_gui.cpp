#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "libpwar.h"
#include "PwarController.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("PWAR");
    app.setApplicationDisplayName("Professional Wireless Audio Router");
    app.setApplicationVersion("1.0");
    app.setDesktopFileName("pwar.desktop");
    
    // Set application icon from resources
    QIcon appIcon(":/icon.png");
    app.setWindowIcon(appIcon);
    
    QQmlApplicationEngine engine;
    PwarController controller;
    
    // Also try setting it in the engine for Wayland compatibility
    engine.rootContext()->setContextProperty("appIcon", appIcon);
    engine.rootContext()->setContextProperty("pwarController", &controller);
    engine.load(QUrl("qrc:/pwar_gui.qml"));
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
