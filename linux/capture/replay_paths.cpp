#include "replay_paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

namespace Shadowless {
namespace {
QString executableName(QString path) {
    path.replace('\\', '/');
    return path.section('/', -1).toLower();
}
const QHash<QString, QString> &catalog() {
    static const auto names = [] {
        QHash<QString, QString> result;
        QFile file(":/shadowless/game-catalog.json");
        if (!file.open(QIODevice::ReadOnly)) return result;
        for (const auto &entry : QJsonDocument::fromJson(file.readAll()).object().value("games").toArray()) {
            const auto game = entry.toObject();
            for (const auto &exe : game.value("e").toArray()) {
                const auto key = executableName(exe.toString());
                const auto name = game.value("n").toString();
                if (result.contains(key) && result.value(key) != name) result[key] = {};
                else if (!result.contains(key)) result.insert(key, name);
            }
        }
        return result;
    }();
    return names;
}
}
QString gameForClients(const QJsonArray &clients) {
    for (const auto &client : clients) {
        const auto window = client.toObject();
        if (!window.value("mapped").toBool() || window.value("hidden").toBool() || window.value("focusHistoryID").toInt(-1) != 0) continue;
        QStringList candidates{window.value("initialClass").toString(), window.value("class").toString()};
        const int pid = window.value("pid").toInt();
        if (pid > 0) {
            candidates.append(QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget());
            QFile cmdline(QString("/proc/%1/cmdline").arg(pid));
            if (cmdline.open(QIODevice::ReadOnly)) {
                for (const auto &arg : cmdline.read(16384).split('\0')) {
                    const auto text = QString::fromUtf8(arg);
                    if (text.endsWith(".exe", Qt::CaseInsensitive)) candidates.append(text);
                }
            }
        }
        for (const auto &candidate : candidates) {
            const auto name = catalog().value(executableName(candidate));
            if (!name.isEmpty()) return name;
        }
        // Native Steam titles are not necessarily represented in the Windows catalog.
        if (window.value("initialClass").toString().startsWith("steam_app_")) {
            const auto title = window.value("initialTitle").toString().trimmed();
            if (!title.isEmpty()) return title;
        }
        return {};
    }
    return {};
}
QString currentGame() {
    QProcess query;
    query.start("hyprctl", {"-j", "clients"});
    if (!query.waitForStarted(500) || !query.waitForFinished(1000)) {
        query.kill(); query.waitForFinished(500); return {};
    }
    if (query.exitStatus() != QProcess::NormalExit || query.exitCode() != 0) return {};
    return gameForClients(QJsonDocument::fromJson(query.readAllStandardOutput()).array());
}
QString routeClip(const QString &root, const QString &path, const QString &game, QString *error) {
    error->clear();
    if (game.trimmed().isEmpty()) return path;
    QString name = game.trimmed();
    name.replace(QRegularExpression("[\\x00-\\x1f/\\\\:*?\"<>|]"), "_");
    name = name.left(80);
    while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
    if (name.isEmpty() || name == "." || name == "..") name = "Game";
    const QFileInfo source(path);
    const QDir folder(root);
    const auto destinationDirectory = folder.filePath(name);
    const QFileInfo target(destinationDirectory);
    if (!source.isFile() || source.isSymLink() || source.canonicalPath() != QFileInfo(root).canonicalFilePath() ||
        target.isSymLink() || !QDir().mkpath(destinationDirectory)) {
        *error = "Clip saved, but the game folder is unavailable."; return path;
    }
    QString destination = QDir(destinationDirectory).filePath(source.fileName());
    if (QFileInfo::exists(destination)) destination = QDir(destinationDirectory).filePath(
        source.completeBaseName() + "_" + QUuid::createUuid().toString(QUuid::Id128) + "." + source.suffix());
    if (!QFile::rename(path, destination)) { *error = "Clip saved, but it could not be moved to the game folder."; return path; }
    return destination;
}
}
