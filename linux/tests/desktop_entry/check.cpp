#include "../../app/desktop_entry.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#include <QProcess>

namespace {
#define check(condition) do { if (!(condition)) qFatal("Check failed at line %d: %s", __LINE__, #condition); } while (false)
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
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qunsetenv("XDG_CURRENT_DESKTOP");
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
    const auto autostart = data.filePath("autostart/io.github.zefth051.Shadowless.desktop");
    check(Shadowless::configureAutostart(true, "/tmp/.mount_shadowless/usr/bin/shadowless").isEmpty());
    check(readFile(autostart).contains("Exec=\"" + appimage.toUtf8() + "\" --background\n"));
    check(!readFile(autostart).contains(".mount_shadowless"));
    check(Shadowless::configureAutostart(false, "/tmp/Shadowless").isEmpty());
    check(!QFile::exists(autostart));
    check(Shadowless::configureAutostart(false, "/tmp/Shadowless").isEmpty());
    qunsetenv("APPIMAGE");
    qputenv("XDG_CURRENT_DESKTOP", "Hyprland");
    check(QDir(data.path()).mkpath("hypr"));
    const auto hypr = data.filePath("hypr/hyprland.lua");
    const QByteArray userConfig("-- My untouched config\nhl.config({general={border_size=2}})\n");
    writeFile(hypr, userConfig);
    const auto quotedApp = data.filePath("Shadowless 'preview' \"double\" \\ $test #1.AppImage");
    writeFile(quotedApp, "#!/bin/sh\nprintf '%s' \"$1\" > \"$SHADOWLESS_STARTUP_TEST_RESULT\"\n");
    check(QFile::setPermissions(quotedApp, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("APPIMAGE", quotedApp.toUtf8());
    check(Shadowless::configureAutostart(true, "/tmp/.mount_shadowless/usr/bin/shadowless").isEmpty());
    const auto luaConfig = readFile(hypr);
    check(luaConfig.startsWith(userConfig));
    check(luaConfig.contains("hl.on(\"hyprland.start\""));
    check(Shadowless::configureAutostart(true, "/tmp/.mount_shadowless/usr/bin/shadowless").isEmpty());
    check(readFile(hypr) == luaConfig);
    const auto luaTest = data.filePath("check.lua");
    writeFile(luaTest, "local callback; hl={config=function() end, on=function(event, fn) assert(event=='hyprland.start'); callback=fn end, exec_cmd=function(command) assert(os.execute(command)) end}; dofile(arg[1]); assert(callback); callback()\n");
    const auto result = data.filePath("started.txt");
    qputenv("SHADOWLESS_STARTUP_TEST_RESULT", result.toUtf8());
    check(QProcess::execute("lua", {luaTest, hypr}) == 0);
    check(readFile(result) == "--background");
    check(Shadowless::configureAutostart(false, "/tmp/Shadowless").isEmpty());
    check(readFile(hypr) == userConfig);
    check(!QFile::exists(autostart));
    const auto linkedConfig = data.filePath("maintained-hyprland.lua");
    check(QFile::rename(hypr, linkedConfig));
    check(QFile::link(linkedConfig, hypr));
    check(Shadowless::configureAutostart(true, "/tmp/Shadowless").isEmpty());
    check(QFileInfo(hypr).isSymLink());
    check(readFile(linkedConfig).contains("BEGIN SHADOWLESS AUTOSTART"));
    check(Shadowless::configureAutostart(false, "/tmp/Shadowless").isEmpty());
    check(readFile(linkedConfig) == userConfig);
    check(QFileInfo(hypr).isSymLink());
    check(QFile::remove(hypr));
    const auto legacy = data.filePath("hypr/hyprland.conf");
    const QByteArray legacyContent("# User config\nmonitor = ,preferred,auto,1\n");
    writeFile(legacy, legacyContent);
    check(Shadowless::configureAutostart(true, "/tmp/Shadowless").isEmpty());
    const auto legacyConfig = readFile(legacy);
    check(legacyConfig.startsWith(legacyContent));
    check(legacyConfig.contains("exec-once = "));
    for (const auto &line : legacyConfig.split('\n')) if (line.startsWith("exec-once = "))
        check(QProcess::execute("/bin/sh", {"-c", QString::fromUtf8(line.mid(12))}) == 0);
    check(readFile(result) == "--background");
    check(Shadowless::configureAutostart(false, "/tmp/Shadowless").isEmpty());
    check(readFile(legacy) == legacyContent);
    check(QFile::remove(legacy));
    writeFile(hypr, userConfig);
    // A malformed owned block must preserve both files instead of guessing.
    writeFile(hypr, userConfig + "\n-- BEGIN SHADOWLESS AUTOSTART\n");
    check(!Shadowless::configureAutostart(true, "/tmp/Shadowless").isEmpty());
    check(readFile(hypr) == userConfig + "\n-- BEGIN SHADOWLESS AUTOSTART\n");
    check(!QFile::exists(autostart));
    writeFile(hypr, userConfig);
    check(QDir().mkpath(autostart));
    check(!Shadowless::configureAutostart(true, "/tmp/Shadowless").isEmpty());
    check(readFile(hypr) == userConfig);
    return 0;
}
