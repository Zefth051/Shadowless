#include "controller.h"
#include "desktop_entry.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLocalServer>
#include <QLockFile>
#include <QStandardPaths>
#include <cstdio>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("shadowless"); app.setOrganizationName("Shadowless");
    app.setDesktopFileName("io.github.zefth051.Shadowless"); app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser; parser.setApplicationDescription("Shadowless - An app for clipping"); parser.addHelpOption();
    for (const auto &option : {"background", "toggle", "show", "hide", "save", "capture", "quit"})
        parser.addOption(QCommandLineOption(option));
    parser.process(app);
    QString command = "toggle";
    for (const auto &option : {"toggle", "show", "hide", "save", "capture", "quit"}) if (parser.isSet(option)) command = option;
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) { fprintf(stderr, "Shadowless requires a private XDG runtime directory.\n"); return 1; }
    const auto socketPath = QDir(runtime).filePath("shadowless-linux.sock");
    QLocalSocket peer; peer.connectToServer(socketPath);
    if (peer.waitForConnected(500)) {
        if (!parser.isSet("background")) { peer.write(command.toUtf8() + '\n'); peer.waitForBytesWritten(1000); }
        return 0;
    }
    // Allow an older running build to finish before using the renamed control socket.
    peer.abort();
    peer.connectToServer(QDir(runtime).filePath("captail-linux.sock"));
    if (peer.waitForConnected(500)) {
        if (parser.isSet("quit")) {
            peer.write("quit\n");
            peer.waitForBytesWritten(1000);
            return 0;
        }
        fprintf(stderr, "Quit the previous Shadowless build before starting this one.\n");
        return 1;
    }
    if (parser.isSet("quit") || parser.isSet("save") || parser.isSet("hide")) { fprintf(stderr, "Shadowless is not running.\n"); return 1; }
    if (QGuiApplication::platformName() != "wayland" || !qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("Hyprland", Qt::CaseInsensitive)) {
        fprintf(stderr, "This Shadowless build requires a native Wayland session on Hyprland.\n"); return 1;
    }
    QLockFile lock(QDir(runtime).filePath("shadowless-linux.lock"));
    if (!lock.tryLock(0)) { fprintf(stderr, "Another Shadowless instance is starting.\n"); return 1; }
    QLocalServer::removeServer(socketPath); QLocalServer server; server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(socketPath)) { fprintf(stderr, "Cannot create Shadowless control socket.\n"); return 1; }
    QString error; const auto capabilities = Shadowless::GsrCapabilities::probe(&error);
    const auto desktopError = Shadowless::ensureDesktopEntry(QCoreApplication::applicationFilePath());
    if (!desktopError.isEmpty()) error += "\n" + desktopError;
    Shadowless::Controller controller(capabilities, error, parser.isSet("background"));
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (auto *client = server.nextPendingConnection()) {
            client->setParent(&server);
            auto *timeout = new QTimer(client); timeout->setSingleShot(true); timeout->start(1000);
            QObject::connect(timeout, &QTimer::timeout, client, &QLocalSocket::deleteLater);
            QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
            QObject::connect(client, &QLocalSocket::readyRead, &app, [client, &controller] {
                if (client->bytesAvailable() > 128) { client->disconnectFromServer(); return; }
                if (!client->canReadLine()) return;
                controller.command(QString::fromUtf8(client->readLine()).trimmed()); client->disconnectFromServer();
            });
        }
    });
    return app.exec();
}
