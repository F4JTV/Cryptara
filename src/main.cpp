// main.cpp - CRYPTARA, secure VARA chat client (C++ / Qt Quick version).
#include "app/Controller.h"
#include "app/I18n.h"
#include "UiHelper.h"
#include "version.h"

#include <QFont>
#include <cstdio>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("CRYPTARA"));
    QGuiApplication::setOrganizationName(QStringLiteral("CRYPTARA"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CRYPTARA_VERSION_STRING));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/cryptara.png")));
    // Material, as the AX25Chat themes are built on it.  A --style argument
    // still wins, as QApplication allowed in the Python version.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE") && !app.arguments().contains(QStringLiteral("-style"))
        && !app.arguments().contains(QStringLiteral("--style")))
        QQuickStyle::setStyle(QStringLiteral("Material"));

    if (app.arguments().contains(QStringLiteral("--version"))) {
        std::printf("CRYPTARA %s\n", CRYPTARA_VERSION_STRING);
        return 0;
    }

    Controller controller;
    qmlRegisterSingletonInstance("CryptaraBackend", 1, 0, "Controller", &controller);
    qmlRegisterSingletonInstance("CryptaraBackend", 1, 0, "I18n", &I18n::instance());
    UiHelper ui;
    qmlRegisterSingletonInstance("CryptaraBackend", 1, 0, "Ui", &ui);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // --screenshot DIR: render each theme into DIR and quit (documentation
    // and checks).  Settings are not loaded nor saved in that mode.
    const int shotIndex = app.arguments().indexOf(QStringLiteral("--screenshot"));
    if (shotIndex >= 0 && shotIndex + 1 < app.arguments().size()) {
        const QString dir = app.arguments().at(shotIndex + 1);
        QObject *root = engine.rootObjects().first();
        QMetaObject::invokeMethod(root, "screenshotMode", Q_ARG(QVariant, QVariant(dir)));
    } else {
        controller.start();
    }
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &controller, &Controller::shutdown);
    return app.exec();
}
