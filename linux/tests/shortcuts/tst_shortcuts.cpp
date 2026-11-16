#include "../../platform/hyprland/shortcuts.h"
#include "../../platform/hyprland/desktop_session.h"
#include <QtTest>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QFile>
#include <QTemporaryDir>
using namespace Shadowless;

// Replace only the external portal on a private bus, keeping the real DBus transport.
class Portal : public QDBusVirtualObject {
public:
    QList<PortalShortcuts> bindings;
    QStringList sessions, closed;
    bool rejectBindings = false;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override {
        if (message.member() == "Register") { bus.send(message.createReply()); return true; }
        if (message.member() == "Close") {
            if (message.interface() == "org.freedesktop.portal.Session") closed << message.path();
            bus.send(message.createReply()); return true;
        }
        const auto args = message.arguments();
        QVariantMap results;
        if (message.member() == "CreateSession") {
            sessions << "/org/freedesktop/portal/desktop/session/test/s" + QString::number(sessions.size());
            results.insert("session_handle", sessions.last());
        } else if (message.member() == "BindShortcuts") {
            bindings << qdbus_cast<PortalShortcuts>(args[1]);
            const PortalShortcuts assigned{{"toggle-overlay", {{"trigger_description", "Meta+K"}}},
                                           {"save-replay", {{"trigger_description", "Ctrl+F9"}}}};
            results.insert("shortcuts", QVariant::fromValue(assigned));
        } else return false;
        QString sender = message.service().mid(1); sender.replace('.', '_');
        const auto token = qdbus_cast<QVariantMap>(args.last()).value("handle_token").toString();
        const auto path = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
        bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath(path))));
        const uint code = rejectBindings && message.member() == "BindShortcuts" ? 1 : 0;
        QTimer::singleShot(1, this, [bus, path, results, code] {
            auto signal = QDBusMessage::createSignal(path, "org.freedesktop.portal.Request", "Response");
            signal.setArguments({code, results}); bus.send(signal);
        });
        return true;
    }
};
class ShortcutTests : public QObject {
    Q_OBJECT
private slots:
    void supportedSessions_data() {
        QTest::addColumn<QString>("platform"); QTest::addColumn<QString>("desktop"); QTest::addColumn<bool>("supported");
        QTest::newRow("Hyprland") << "wayland" << "Hyprland" << true;
        QTest::newRow("Plasma") << "wayland" << "KDE" << true;
        QTest::newRow("desktop-list") << "wayland" << "plasma:KDE" << true;
        QTest::newRow("X11") << "xcb" << "KDE" << false;
        QTest::newRow("GNOME") << "wayland" << "GNOME" << false;
        QTest::newRow("substring") << "wayland" << "NotHyprland" << false;
        QTest::newRow("missing") << "wayland" << "" << false;
    }
    void supportedSessions() {
        QFETCH(QString, platform); QFETCH(QString, desktop); QFETCH(bool, supported);
        QCOMPARE(isSupportedWaylandDesktop(platform, desktop), supported);
    }
    void portalUsesConfiguredKeysAndRecreatesSession() {
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        qputenv("HYPRLAND_INSTANCE_SIGNATURE", "stale");
        QTemporaryDir tools;
        const auto log = tools.filePath("hyprctl-called");
        QFile command(tools.filePath("hyprctl")); QVERIFY(command.open(QIODevice::WriteOnly));
        command.write(("#!/bin/sh\necho called >> '" + log + "'\necho '[]'\n").toUtf8()); command.close();
        QVERIFY(command.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("PATH", tools.path().toUtf8());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService("org.freedesktop.portal.Desktop"));
        Portal portal;
        QVERIFY(bus.registerVirtualObject("/org/freedesktop/portal/desktop", &portal, QDBusConnection::SubPath));
        {
            Shortcuts shortcuts;
            QSignalSpy status(&shortcuts, &Shortcuts::status);
            shortcuts.setKeys({"Meta+K", "Ctrl+F9", "Alt+Space"});
            shortcuts.start();
            QTRY_COMPARE(portal.bindings.size(), 1);
            QCOMPARE(portal.bindings[0][0].properties.value("preferred_trigger").toString(), QString("LOGO+k"));
            QCOMPARE(portal.bindings[0][1].properties.value("preferred_trigger").toString(), QString("CTRL+F9"));
            QCOMPARE(portal.bindings[0][2].properties.value("preferred_trigger").toString(), QString("ALT+space"));
            QTRY_VERIFY(!status.isEmpty());
            QVERIFY(status.last()[0].toString().contains("Meta+K"));
            QVERIFY(status.last()[0].toString().contains("Ctrl+F9"));
            QVERIFY(status.last()[0].toString().contains("not assigned"));
            shortcuts.setKeys({"Alt+J", "Ctrl+F8", "Alt+F9"});
            QTRY_COMPARE(portal.bindings.size(), 2);
            QCOMPARE(portal.bindings[1][0].properties.value("preferred_trigger").toString(), QString("ALT+j"));
            QTRY_VERIFY(portal.closed.contains(portal.sessions[0]));
            QVERIFY(portal.sessions[0] != portal.sessions[1]);
            QTRY_COMPARE(status.size(), 2);
            auto changed = QDBusMessage::createSignal("/org/freedesktop/portal/desktop", "org.freedesktop.portal.GlobalShortcuts", "ShortcutsChanged");
            changed.setArguments({QVariant::fromValue(QDBusObjectPath(portal.sessions[1])),
                                  QVariant::fromValue(PortalShortcuts{{"toggle-overlay", {{"trigger_description", "Alt+J"}}}})});
            QVERIFY(bus.send(changed));
            QTRY_COMPARE(status.size(), 3);
            QVERIFY(status.last()[0].toString().contains("Alt+J"));
            QSignalSpy activation(&shortcuts, &Shortcuts::activated);
            for (const auto &session : portal.sessions) {
                auto signal = QDBusMessage::createSignal("/org/freedesktop/portal/desktop", "org.freedesktop.portal.GlobalShortcuts", "Activated");
                signal.setArguments({QVariant::fromValue(QDBusObjectPath(session)), "toggle-overlay", QVariant::fromValue(qulonglong(0)), QVariantMap{}});
                QVERIFY(bus.send(signal));
            }
            QTRY_COMPARE(activation.size(), 1);
            portal.rejectBindings = true;
            shortcuts.setKeys({"Alt+L", "Ctrl+F8", "Alt+F9"});
            QTRY_COMPARE(portal.bindings.size(), 3);
            QTRY_VERIFY(status.last()[0].toString().contains("cancelled or rejected"));
        }
        QTRY_COMPARE(portal.closed.size(), 3);
        QVERIFY2(!QFile::exists(log), "Plasma must never run hyprctl, including on destruction");
        bus.unregisterObject("/org/freedesktop/portal/desktop", QDBusConnection::UnregisterTree);
        bus.unregisterService("org.freedesktop.portal.Desktop");
    }
};
QTEST_GUILESS_MAIN(ShortcutTests)
#include "tst_shortcuts.moc"
