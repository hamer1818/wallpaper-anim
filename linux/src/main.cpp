#include "app.h"
#include "config.h"
#include "localization.h"
#include "version.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSurfaceFormat>
#include <QSystemTrayIcon>

#include <unistd.h>

namespace {

QString instanceSocketName()
{
    return QStringLiteral("wallpaperanim-%1").arg(getuid());
}

// Hands an already-running instance a command instead of starting a second one.
bool forwardToRunningInstance(const QByteArray& command)
{
    QLocalSocket socket;
    socket.connectToServer(instanceSocketName());
    if (!socket.waitForConnected(400)) return false;

    socket.write(command);
    socket.flush();
    socket.waitForBytesWritten(400);
    socket.disconnectFromServer();
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    // mpv's render API and the shader renderer both want a modern core profile.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication qtApp(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("WallpaperAnim"));
    QCoreApplication::setOrganizationName(QStringLiteral("WallpaperAnim"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STRING));
    QGuiApplication::setDesktopFileName(QStringLiteral("wallpaperanim"));
    // Closing the settings window must not end the session: the wallpaper lives on.
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("WallpaperAnim - animated wallpaper engine"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption backgroundOption(QStringLiteral("background"),
                                        QStringLiteral("Start without opening the settings window."));
    QCommandLineOption toggleOption(QStringLiteral("toggle"),
                                    QStringLiteral("Pause/resume the running instance."));
    QCommandLineOption nextOption(QStringLiteral("next"),
                                  QStringLiteral("Switch the running instance to the next wallpaper."));
    QCommandLineOption quitOption(QStringLiteral("quit"), QStringLiteral("Quit the running instance."));
    parser.addOption(backgroundOption);
    parser.addOption(toggleOption);
    parser.addOption(nextOption);
    parser.addOption(quitOption);
    parser.process(qtApp);

    QByteArray forwardCommand = "show";
    if (parser.isSet(toggleOption)) forwardCommand = "toggle";
    else if (parser.isSet(nextOption)) forwardCommand = "next";
    else if (parser.isSet(quitOption)) forwardCommand = "quit";

    if (forwardToRunningInstance(forwardCommand)) {
        return 0;
    }
    if (parser.isSet(toggleOption) || parser.isSet(nextOption) || parser.isSet(quitOption)) {
        qWarning() << "WallpaperAnim is not running.";
        return 1;
    }

    // A stale socket file is left behind by a crash; clear it before listening.
    QLocalServer::removeServer(instanceSocketName());
    QLocalServer instanceServer;
    if (!instanceServer.listen(instanceSocketName())) {
        qWarning() << "WallpaperAnim: cannot listen on the single-instance socket:"
                   << instanceServer.errorString();
    }

    Config::ConfigManager::GetInstance().Load();

    App engine;

    QObject::connect(&instanceServer, &QLocalServer::newConnection, &engine, [&instanceServer, &engine]() {
        QLocalSocket* client = instanceServer.nextPendingConnection();
        if (!client) return;
        QObject::connect(client, &QLocalSocket::readyRead, &engine, [client, &engine]() {
            const QByteArray command = client->readAll().trimmed();
            if (command == "toggle") {
                engine.SetUserPaused(!engine.IsUserPaused());
            } else if (command == "next") {
                engine.NextWallpaper();
            } else if (command == "quit") {
                QCoreApplication::quit();
            } else {
                engine.ShowSettings();
            }
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });

    engine.Start(!parser.isSet(backgroundOption));

    return qtApp.exec();
}
