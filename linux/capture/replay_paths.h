#pragma once
#include <QJsonArray>
#include <QString>
namespace Shadowless {
QString gameForClients(const QJsonArray &clients);
QString currentGame();
QString routeClip(const QString &root, const QString &path, const QString &game, QString *error);
}
