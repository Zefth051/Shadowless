#pragma once
#include <QObject>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QTimer>
#include <QLocalSocket>
#include "bindings.h"

namespace Shadowless {
struct PortalShortcut { QString id; QVariantMap properties; };
using PortalShortcuts = QList<PortalShortcut>;
QDBusArgument &operator<<(QDBusArgument &, const PortalShortcut &);
const QDBusArgument &operator>>(const QDBusArgument &, PortalShortcut &);
class Shortcuts : public QObject {
    Q_OBJECT
public:
    explicit Shortcuts(QObject *parent = nullptr);
    ~Shortcuts() override;
    void start();
    void setKeys(const QStringList &keys);
signals:
    void activated(const QString &action);
    void status(const QString &message);
private slots:
    void response(uint code, const QVariantMap &results, const QDBusMessage &message);
    void triggered(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &options);
private:
    QString m_request, m_session;
    QDBusConnection m_bus;
    int m_stage = 0;
    QTimer m_timeout;
    QStringList m_keys{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"};
    QVector<HyprBinding> m_owned;
    QLocalSocket m_events;
    QByteArray m_eventBuffer;
    bool m_lua = false;
    void call(const QString &method, const QVariantList &args, const QString &token);
    void createSession();
    void installBindings();
    void removeBindings();
};
} // namespace Shadowless
Q_DECLARE_METATYPE(Shadowless::PortalShortcut)
Q_DECLARE_METATYPE(Shadowless::PortalShortcuts)
