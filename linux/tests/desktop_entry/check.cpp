#include "../../app/desktop_entry.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>

namespace {
void check(bool condition) { if (!condition) std::abort(); }
void writeFile(const QString &path, const QByteArray &content) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly));
    check(file.write(content) == content.size());
}
QByteArray readFile(const QString &path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly));
    return file.readAll();
}
}

int main(int argc, char **argv) {
    QTemporaryDir data;
    check(data.isValid());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    QCoreApplication app(argc, argv);
    check(QDir(data.path()).mkpath("applications"));
    const auto desktop = data.filePath("applications/io.github.zefth051.Shadowless.desktop");
    const auto icon = data.filePath("icons/io.github.zefth051.Shadowless.png");
    const QByteArray unrelated("[Desktop Entry]\nName=Someone else\n");
    writeFile(desktop, unrelated);
    check(Shadowless::ensureDesktopEntry("/tmp/Shadowless").isEmpty());
    check(readFile(desktop) == unrelated);
    check(!QFile::exists(icon));

    writeFile(desktop, "[Desktop Entry]\nComment=Native capture and replay overlay\nIcon=video-display\n");
    check(Shadowless::ensureDesktopEntry("/tmp/Shadowless").isEmpty());
    const auto registered = readFile(desktop);
    check(registered.contains("Icon=" + icon.toUtf8() + "\n"));
    check(readFile(icon) == readFile(":/shadowless/moon.png"));
    check(Shadowless::ensureDesktopEntry("/tmp/Shadowless").isEmpty());
    check(readFile(desktop) == registered);
    check(QFile::remove(desktop));
    check(Shadowless::ensureDesktopEntry("/tmp/Shadowless").isEmpty());
    check(readFile(desktop) == registered);
    const auto appimage = data.filePath("Shadowless preview.AppImage");
    writeFile(appimage, "fixture");
    qputenv("APPIMAGE", appimage.toUtf8());
    check(Shadowless::ensureDesktopEntry("/tmp/.mount_shadowless/usr/bin/shadowless").isEmpty());
    check(readFile(desktop).contains("Exec=\"" + appimage.toUtf8() + "\" --toggle\n"));
    check(!readFile(desktop).contains(".mount_shadowless"));
    qunsetenv("APPIMAGE");
    return 0;
}
