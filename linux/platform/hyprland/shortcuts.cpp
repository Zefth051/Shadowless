#include "shortcuts.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>

namespace Shadowless {
static const QString service = "org.freedesktop.portal.Desktop", path = "/org/freedesktop/portal/desktop", interface = "org.freedesktop.portal.GlobalShortcuts";
QDBusArgument &operator<<(QDBusArgument &a, const PortalShortcut &s) {
    a.beginStructure(); a << s.id << s.properties; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, PortalShortcut &s) {
    a.beginStructure(); a >> s.id >> s.properties; a.endStructure(); return a;
}
Shortcuts::Shortcuts(QObject *parent) : QObject(parent), m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, "shadowless-shortcuts")) {
    qDBusRegisterMetaType<PortalShortcut>(); qDBusRegisterMetaType<PortalShortcuts>();
    auto bus = m_bus;
    bus.connect(service, {}, "org.freedesktop.portal.Request", "Response", this, SLOT(response(uint,QVariantMap,QDBusMessage)));
    const bool connected = bus.connect(service, path, interface, "Activated", this, SLOT(triggered(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    if (!connected) qWarning() << "Shadowless shortcut subscription failed:" << bus.lastError();
    m_timeout.setSingleShot(true);
    connect(this, &Shortcuts::status, this, [](const QString &message) { qInfo().noquote() << "Shadowless shortcuts:" << message; });
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        emit status("Global shortcut setup is still pending. Open Shadowless from the tray or use --toggle.");
    });
    connect(&m_events, &QLocalSocket::readyRead, this, [this] {
        m_eventBuffer += m_events.readAll();
        if (m_eventBuffer.size() > 65536) { m_eventBuffer.clear(); return; }
        while (m_eventBuffer.contains('\n')) {
            const auto end = m_eventBuffer.indexOf('\n'); const auto line = m_eventBuffer.left(end); m_eventBuffer.remove(0, end + 1);
            if (line.startsWith("configreloaded>>") && m_stage == 3) { m_owned.clear(); installBindings(); }
        }
    });
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const auto signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (!signature.isEmpty()) m_events.connectToServer(QDir(runtime).filePath("hypr/" + signature + "/.socket2.sock"));
}
Shortcuts::~Shortcuts() {
    removeBindings();
    if (!m_session.isEmpty()) {
        auto message = QDBusMessage::createMethodCall(service, m_session, "org.freedesktop.portal.Session", "Close");
        m_bus.asyncCall(message);
    }
}
void Shortcuts::call(const QString &method, const QVariantList &args, const QString &token) {
    auto bus = m_bus;
    QString sender = bus.baseService().mid(1); sender.replace('.', '_');
    m_request = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
    auto message = QDBusMessage::createMethodCall(service, path, interface, method); message.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) { m_timeout.stop(); emit status("Global shortcuts unavailable: " + reply.error().message() + ". Use the tray or --toggle."); }
        watcher->deleteLater();
    });
    m_timeout.start(30000);
}
void Shortcuts::start() {
    if (m_stage) return;
    m_stage = 1;
    auto registration = QDBusMessage::createMethodCall(service, path, "org.freedesktop.host.portal.Registry", "Register");
    registration.setArguments({"io.github.zefth051.Shadowless", QVariantMap{}});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(registration), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError()) emit status("Cannot register Shadowless's shortcut identity: " + reply.error().message());
        else createSession();
        watcher->deleteLater();
    });
}
void Shortcuts::createSession() {
    const auto token = "shadowless_" + QUuid::createUuid().toString(QUuid::Id128);
    call("CreateSession", {QVariantMap{{"handle_token", token}, {"session_handle_token", token}}}, token);
}
void Shortcuts::response(uint code, const QVariantMap &results, const QDBusMessage &message) {
    if (message.path() != m_request) return;
    m_timeout.stop();
    if (code != 0) { emit status("Global shortcut setup was cancelled or rejected. Use the tray or --toggle."); return; }
    if (m_stage == 1) {
        m_session = results.value("session_handle").toString();
        if (m_session.isEmpty()) { emit status("The shortcut portal returned no session."); return; }
        m_stage = 2;
        const auto token = "shadowless_" + QUuid::createUuid().toString(QUuid::Id128);
        const PortalShortcuts shortcuts{
            {"toggle-overlay", {{"description", "Show or hide Shadowless"}, {"preferred_trigger", "ALT+z"}}},
            {"save-replay", {{"description", "Save Shadowless replay"}, {"preferred_trigger", "CTRL+SHIFT+F10"}}},
            {"toggle-capture", {{"description", "Start or stop Shadowless capture"}, {"preferred_trigger", "ALT+F10"}}}};
        call("BindShortcuts", {QVariant::fromValue(QDBusObjectPath(m_session)), QVariant::fromValue(shortcuts), QString{}, QVariantMap{{"handle_token", token}}}, token);
    } else {
        m_stage = 3;
        installBindings();
    }
}
void Shortcuts::triggered(const QDBusObjectPath &session, const QString &id, qulonglong, const QVariantMap &) {
    if (session.path() == m_session) emit activated(id);
}
static QByteArray hyprctl(const QStringList &arguments) {
    QProcess process; process.start("hyprctl", arguments);
    if (!process.waitForStarted(500) || !process.waitForFinished(1000) || process.exitCode() != 0) return {};
    return process.readAllStandardOutput();
}
void Shortcuts::setKeys(const QStringList &keys) {
    if (keys.size() != 3 || keys == m_keys) return;
    removeBindings(); m_keys = keys; if (m_stage == 3) installBindings();
}
void Shortcuts::installBindings() {
    QJsonParseError error; auto document = QJsonDocument::fromJson(hyprctl({"-j", "binds"}), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) { emit status("Cannot inspect Hyprland bindings. Use the tray or --toggle."); return; }
    const QStringList ids{"toggle-overlay", "save-replay", "toggle-capture"};
    const QStringList labels{"Overlay", "Save replay", "Capture"}; QStringList messages;
    auto bindings = document.array();
    for (int i = 0; i < 3; ++i) {
        HyprBinding binding;
        if (!HyprBinding::parse(m_keys[i], ids[i], &binding)) { messages << labels[i] + ": unsupported shortcut"; continue; }
        const auto available = binding.availability(bindings);
        if (available == HyprBinding::Conflict) { messages << labels[i] + ": " + m_keys[i] + " is already in use"; continue; }
        if (available == HyprBinding::Free) {
            QByteArray reply;
            if (!m_lua) reply = hyprctl({"keyword", "bindp", binding.modifiers + "," + binding.key + ",global,io.github.zefth051.Shadowless:" + binding.id});
            if (reply.contains("non-legacy parsers")) m_lua = true;
            if (m_lua) {
                QStringList parts = binding.modifiers.split(' ', Qt::SkipEmptyParts); parts << binding.key;
                const QString code = "__shadowless_native_bindings = __shadowless_native_bindings or {}; __shadowless_native_bindings[\"" + binding.id +
                    "\"] = hl.bind(\"" + parts.join(" + ") + "\", hl.dsp.global(\"io.github.zefth051.Shadowless:" + binding.id +
                    "\"), {dont_inhibit=true, submap_universal=true, description=\"Shadowless native: " + binding.id + "\"})";
                reply = hyprctl({"eval", code});
            }
            if (reply.trimmed() != "ok") { messages << labels[i] + ": registration failed"; continue; }
            m_owned.append(binding);
        }
        messages << labels[i] + ": " + m_keys[i];
    }
    emit status(messages.join(" · "));
}
void Shortcuts::removeBindings() {
    const auto bindings = QJsonDocument::fromJson(hyprctl({"-j", "binds"})).array();
    for (const auto &binding : m_owned) {
        if (m_lua) {
            const auto reference = "__shadowless_native_bindings[\"" + binding.id + "\"]";
            hyprctl({"eval", "if __shadowless_native_bindings and " + reference + " then " + reference + ":remove(); " + reference + "=nil end"});
            continue;
        }
        int matches = 0;
        for (const auto &value : bindings) { const auto row = value.toObject(); if (row.value("modmask").toInt() == binding.mask && row.value("key").toString().compare(binding.key, Qt::CaseInsensitive) == 0) ++matches; }
        if (matches == 1 && binding.availability(bindings) == HyprBinding::Existing)
            hyprctl({"keyword", "unbind", binding.modifiers + "," + binding.key});
    }
    m_owned.clear();
}
} // namespace Shadowless
