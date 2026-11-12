#include "desktop_entry.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace Shadowless {
namespace {
QString launchPath(const QString &executable) {
    const QFileInfo appimage(qEnvironmentVariable("APPIMAGE"));
    return appimage.isAbsolute() && appimage.isFile() ? appimage.absoluteFilePath() : executable;
}
QString desktopExecutable(const QString &executable) {
    QString escaped = launchPath(executable);
    escaped.replace('\\', "\\\\"); escaped.replace('"', "\\\""); escaped.replace('`', "\\`"); escaped.replace('$', "\\$"); escaped.replace('%', "%%");
    return escaped;
}
bool saveFile(const QString &path, const QByteArray &content) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size() && file.commit();
}
QString prepareHyprlandAutostart(bool enabled, const QString &executable, QString *path, QByteArray *previous, QByteArray *updated) {
    if (enabled && !qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("Hyprland", Qt::CaseInsensitive)) return {};
    const auto directory = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("hypr");
    const bool lua = QFileInfo::exists(QDir(directory).filePath("hyprland.lua"));
    const auto candidate = QDir(directory).filePath(lua ? "hyprland.lua" : "hyprland.conf");
    if (!QFileInfo::exists(candidate)) {
        if (enabled && qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("Hyprland", Qt::CaseInsensitive))
            return "Cannot find the Hyprland configuration for launch at login. Use the default hyprland.lua or hyprland.conf path.";
        return {};
    }
    *path = QFileInfo(candidate).canonicalFilePath();
    QFile file(*path);
    if (!file.open(QIODevice::ReadOnly)) return "Cannot read Hyprland configuration: " + *path;
    *previous = file.readAll();
    const QByteArray prefix = lua ? "--" : "#";
    const auto begin = '\n' + prefix + " BEGIN SHADOWLESS AUTOSTART\n";
    const auto end = prefix + " END SHADOWLESS AUTOSTART\n";
    *updated = *previous;
    const auto start = updated->indexOf(begin);
    const auto finish = updated->indexOf(end);
    if ((start < 0) != (finish < 0) || (start >= 0 && (finish < start || updated->indexOf(begin, start + begin.size()) >= 0 || updated->indexOf(end, finish + end.size()) >= 0)))
        return "The Shadowless autostart block in the Hyprland configuration is incomplete. Restore it before changing launch at login.";
    if (start >= 0) updated->remove(start, finish + end.size() - start);
    if (enabled) {
        QString quoted = launchPath(executable);
        quoted.replace('\'', "'\\''");
        const QString command = "exec '" + quoted + "' --background";
        QByteArray instruction;
        if (lua) {
            QString escaped = command;
            escaped.replace('\\', "\\\\"); escaped.replace('"', "\\\""); escaped.replace('\t', "\\t");
            instruction = "hl.on(\"hyprland.start\", function() hl.exec_cmd(\"" + escaped.toUtf8() + "\") end)\n";
        } else {
            // Encode paths so hyprlang cannot expand their variables or comment characters.
            instruction = "exec-once = /bin/sh -c 'printf %s " + command.toUtf8().toBase64() + " | base64 -d | /bin/sh'\n";
        }
        *updated += begin + instruction + end;
    }
    return {};
}
}
QString configureAutostart(bool enabled, const QString &executable) {
    const auto directory = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("autostart");
    const auto path = QDir(directory).filePath("io.github.zefth051.Shadowless.desktop");
    if (executable.contains('\n') || executable.contains('\r') || qEnvironmentVariable("APPIMAGE").contains('\n') || qEnvironmentVariable("APPIMAGE").contains('\r'))
        return "The launch path contains an unsupported line break.";
    QString hyprPath; QByteArray previousHypr, updatedHypr;
    const auto error = prepareHyprlandAutostart(enabled, executable, &hyprPath, &previousHypr, &updatedHypr);
    if (!error.isEmpty()) return error;
    if (enabled && !QDir().mkpath(directory)) return "Cannot create the autostart directory: " + directory;
    const bool hyprChanged = !hyprPath.isEmpty() && previousHypr != updatedHypr;
    if (hyprChanged && !saveFile(hyprPath, updatedHypr)) return "Cannot update Hyprland autostart: " + hyprPath;
    const auto content = QString("[Desktop Entry]\nType=Application\nName=Shadowless\nExec=\"%1\" --background\nIcon=io.github.zefth051.Shadowless\nTerminal=false\n").arg(desktopExecutable(executable)).toUtf8();
    const bool saved = enabled ? saveFile(path, content) : !QFile::exists(path) || QFile::remove(path);
    if (!saved) {
        const bool restored = !hyprChanged || saveFile(hyprPath, previousHypr);
        return "Cannot update launch at login: " + path + (restored ? QString{} : "\nCould not restore Hyprland configuration: " + hyprPath);
    }
    return {};
}
QString ensureDesktopEntry(const QString &executable) {
    const auto dataDirectory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const auto directory = QDir(dataDirectory).filePath("applications");
    const auto path = QDir(directory).filePath("io.github.zefth051.Shadowless.desktop");
    QFile existing(path);
    QByteArray previous;
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly)) return "Cannot inspect desktop identity: " + path;
        previous = existing.readAll();
        if (!previous.contains("\nComment=Native capture and replay overlay\n")) return {};
        existing.close();
    }
    const auto iconDirectory = QDir(dataDirectory).filePath("icons");
    const auto iconPath = QDir(iconDirectory).filePath("io.github.zefth051.Shadowless.png");
    if (!QDir().mkpath(iconDirectory)) return "Cannot create Shadowless's icon directory.";
    QFile source(":/shadowless/moon.png");
    if (!source.open(QIODevice::ReadOnly)) return "Cannot read Shadowless's moon icon.";
    const auto icon = source.readAll();
    QSaveFile installedIcon(iconPath);
    if (!installedIcon.open(QIODevice::WriteOnly) || installedIcon.write(icon) != icon.size() || !installedIcon.commit())
        return "Cannot install desktop icon: " + iconPath;
    // Keep the portal identity so existing permissions and shortcuts survive the rename.
    if (!QDir().mkpath(directory)) return "Cannot create Shadowless's desktop identity directory.";
    const auto escaped = desktopExecutable(executable);
    QSaveFile file(path);
    QString escapedIcon = iconPath;
    escapedIcon.replace('\\', "\\\\"); escapedIcon.replace('\n', "\\n"); escapedIcon.replace('\r', "\\r");
    const QByteArray content = QString("[Desktop Entry]\nType=Application\nName=Shadowless\nComment=Native capture and replay overlay\nExec=\"%1\" --toggle\nIcon=%2\nTerminal=false\nCategories=AudioVideo;Recorder;\nNoDisplay=true\n").arg(escaped, escapedIcon).toUtf8();
    if (previous == content) return {};
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) return "Cannot register desktop identity: " + path;
    return {};
}
} // namespace Shadowless
