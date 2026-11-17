#pragma once
#include <QJsonArray>
#include <QString>
namespace Shadowless {
QString gameForClients(const QJsonArray &clients);
QString gameForProcesses(const QString &processDirectory);
QString currentGame();
bool isKnownAudioGame(const QString &name);
QString routeClip(const QString &root, const QString &path, const QString &game, QString *error);
}
