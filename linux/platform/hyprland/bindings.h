#pragma once
#include <QJsonArray>
#include <QString>
#include <QStringList>
namespace Shadowless {
QStringList legacyBindingCleanup(const QJsonArray &bindings);
struct HyprBinding {
    QString modifiers, key, id;
    int mask = 0;
    static bool parse(const QString &sequence, const QString &id, HyprBinding *binding);
    enum Availability { Free, Existing, Conflict };
    Availability availability(const QJsonArray &bindings) const;
};
} // namespace Shadowless
