#include "desktop_entry.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace Shadowless {
QString ensureDesktopEntry(const QString &executable) {
    const auto directory = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("applications");
    const auto path = QDir(directory).filePath("io.github.zefth051.Shadowless.desktop");
    // Keep the portal identity so existing permissions and shortcuts survive the rename.
    if (!QDir().mkpath(directory)) return "Cannot create Shadowless's desktop identity directory.";
    QString escaped = executable; escaped.replace('\\', "\\\\"); escaped.replace('"', "\\\""); escaped.replace('`', "\\`"); escaped.replace('$', "\\$"); escaped.replace('%', "%%");
    QSaveFile file(path);
    const QByteArray content = QString("[Desktop Entry]\nType=Application\nName=Shadowless\nComment=Native capture and replay overlay\nExec=\"%1\" --toggle\nIcon=video-display\nTerminal=false\nCategories=AudioVideo;Recorder;\nNoDisplay=true\n").arg(escaped).toUtf8();
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        const auto previous = existing.readAll();
        if (previous == content || !previous.contains("\nComment=Native capture and replay overlay\n")) return {};
        existing.close();
    }
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) return "Cannot register desktop identity: " + path;
    return {};
}
} // namespace Shadowless
