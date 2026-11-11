#include "desktop_entry.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace Shadowless {
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
    const QFileInfo appimage(qEnvironmentVariable("APPIMAGE"));
    const QString launchPath = appimage.isAbsolute() && appimage.isFile() ? appimage.absoluteFilePath() : executable;
    QString escaped = launchPath; escaped.replace('\\', "\\\\"); escaped.replace('"', "\\\""); escaped.replace('`', "\\`"); escaped.replace('$', "\\$"); escaped.replace('%', "%%");
    QSaveFile file(path);
    QString escapedIcon = iconPath;
    escapedIcon.replace('\\', "\\\\"); escapedIcon.replace('\n', "\\n"); escapedIcon.replace('\r', "\\r");
    const QByteArray content = QString("[Desktop Entry]\nType=Application\nName=Shadowless\nComment=Native capture and replay overlay\nExec=\"%1\" --toggle\nIcon=%2\nTerminal=false\nCategories=AudioVideo;Recorder;\nNoDisplay=true\n").arg(escaped, escapedIcon).toUtf8();
    if (previous == content) return {};
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) return "Cannot register desktop identity: " + path;
    return {};
}
} // namespace Shadowless
