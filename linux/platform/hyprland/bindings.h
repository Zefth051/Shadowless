#pragma once
#include <QJsonArray>
#include <QString>
namespace Shadowless {
struct HyprBinding {
    QString modifiers, key, id;
    int mask = 0;
    static bool parse(const QString &sequence, const QString &id, HyprBinding *binding);
    enum Availability { Free, Existing, Conflict };
    Availability availability(const QJsonArray &bindings) const;
};
} // namespace Shadowless
