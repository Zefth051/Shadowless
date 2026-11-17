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
    void approvalChanged(bool pending);
private slots:
    void response(uint code, const QVariantMap &results, const QDBusMessage &message);
    void triggered(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &options);
    void shortcutsChanged(const QDBusObjectPath &session, const Shadowless::PortalShortcuts &shortcuts);
private:
    QString m_request, m_session;
    QDBusConnection m_bus;
    int m_stage = 0;
    QTimer m_timeout;
    QStringList m_keys{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10", "Alt+F9"};
    QVector<HyprBinding> m_owned;
    QLocalSocket m_events;
    QByteArray m_eventBuffer;
    bool m_lua = false;
    bool m_hyprland = false, m_registered = false, m_portalLost = false;
    void call(const QString &method, const QVariantList &args, const QString &token);
    void createSession();
    void installBindings();
    void removeBindings();
    void closeSession();
    void reportBindings(const PortalShortcuts &shortcuts);
};
} // namespace Shadowless
Q_DECLARE_METATYPE(Shadowless::PortalShortcut)
Q_DECLARE_METATYPE(Shadowless::PortalShortcuts)
