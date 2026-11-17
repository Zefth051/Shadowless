#include "discord_presence.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>
#include <QUuid>
#include <QtEndian>

namespace Shadowless {
namespace {
constexpr quint32 maximumPayload = 64 * 1024;
constexpr auto applicationId = "1571906955054948393";
}
DiscordPresence::DiscordPresence(QObject *parent) : QObject(parent) {
    m_socket.setReadBufferSize(maximumPayload + 8);
    for (auto *timer : {&m_retry, &m_deadline, &m_update}) timer->setSingleShot(true);
    m_retry.setInterval(30000);
    m_deadline.setInterval(10000);
    m_update.setInterval(5000);
    connect(&m_retry, &QTimer::timeout, this, [this] { m_path = 0; connectNext(); });
    connect(&m_deadline, &QTimer::timeout, this, &DiscordPresence::disconnected);
    connect(&m_update, &QTimer::timeout, this, &DiscordPresence::publish);
    connect(&m_socket, &QLocalSocket::connected, this, &DiscordPresence::handshake);
    connect(&m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::read);
    connect(&m_socket, &QLocalSocket::disconnected, this, &DiscordPresence::disconnected);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, &DiscordPresence::disconnected);
}
DiscordPresence::~DiscordPresence() { stop(); }
void DiscordPresence::start() {
    if (m_started) return;
    m_started = true;
    m_paths.clear();
    QStringList directories;
    for (const auto *name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
        const auto path = qEnvironmentVariable(name);
        if (!path.isEmpty() && QDir::isAbsolutePath(path)) directories.append(path);
    }
    directories.append("/tmp");
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty() && QDir::isAbsolutePath(runtime)) {
        directories.append(QDir(runtime).filePath("app/com.discordapp.Discord"));
        directories.append(QDir(runtime).filePath("app/com.discordapp.DiscordCanary"));
    }
    directories.removeDuplicates();
    for (const auto &directory : directories)
        for (int index = 0; index < 10; ++index)
            m_paths.append(QDir(directory).filePath(QString("discord-ipc-%1").arg(index)));
    m_path = 0;
    connectNext();
}
void DiscordPresence::stop() {
    m_started = false;
    m_retry.stop(); m_deadline.stop(); m_update.stop();
    if (m_ready && m_socket.state() == QLocalSocket::ConnectedState) {
        const QJsonObject clear{{"cmd", "SET_ACTIVITY"}, {"nonce", QUuid::createUuid().toString()},
            {"args", QJsonObject{{"pid", QCoreApplication::applicationPid()}, {"activity", QJsonValue::Null}}}};
        write(1, QJsonDocument(clear).toJson(QJsonDocument::Compact));
        m_socket.flush();
    }
    m_ready = false; m_hasSent = false; m_nonce.clear(); m_input.clear();
    const QSignalBlocker blocker(&m_socket);
    m_socket.abort();
}
void DiscordPresence::setState(State state, const QString &game) {
    QString title = state == Idle ? QString() : game.simplified().left(100);
    if (m_desired == state && m_game == title) return;
    m_game = title;
    m_desired = state;
    if (!m_update.isActive()) publish();
}
QString DiscordPresence::stateText(State state) {
    switch (state) {
    case InstantClip: return "Instant Clip enabled";
    case Recording: return "Recording";
    case Paused: return "Recording paused";
    case Unknown: return "Recording state unknown";
    case Idle: return "Idle";
    }
    return "Idle";
}
QByteArray DiscordPresence::frame(quint32 opcode, const QByteArray &payload) {
    if (payload.size() > maximumPayload) return {};
    QByteArray bytes(8, '\0');
    qToLittleEndian(opcode, bytes.data());
    qToLittleEndian(quint32(payload.size()), bytes.data() + 4);
    bytes.append(payload);
    return bytes;
}
bool DiscordPresence::write(quint32 opcode, const QByteArray &payload) {
    const auto bytes = frame(opcode, payload);
    if (bytes.isEmpty() || m_socket.state() != QLocalSocket::ConnectedState
        || m_socket.bytesToWrite() + bytes.size() > 2 * (maximumPayload + 8)) return false;
    return m_socket.write(bytes) == bytes.size();
}
void DiscordPresence::connectNext() {
    if (!m_started || m_socket.state() != QLocalSocket::UnconnectedState) return;
    while (m_path < m_paths.size()) {
        const auto path = m_paths[m_path++];
        if (!QFileInfo::exists(path)) continue;
        m_deadline.start();
        m_socket.connectToServer(path, QIODevice::ReadWrite);
        return;
    }
    m_retry.start();
}
void DiscordPresence::handshake() {
    const QJsonObject hello{{"v", 1}, {"client_id", applicationId}};
    if (!write(0, QJsonDocument(hello).toJson(QJsonDocument::Compact))) { disconnected(); return; }
    m_deadline.start();
}
void DiscordPresence::disconnected() {
    m_ready = false; m_hasSent = false; m_nonce.clear(); m_input.clear();
    m_deadline.stop(); m_update.stop();
    {
        const QSignalBlocker blocker(&m_socket);
        m_socket.abort();
    }
    if (m_started) QTimer::singleShot(0, this, &DiscordPresence::connectNext);
}
void DiscordPresence::read() {
    while (m_socket.bytesAvailable()) {
        m_input.append(m_socket.read(maximumPayload + 8 - m_input.size()));
        while (m_input.size() >= 8) {
            const auto opcode = qFromLittleEndian<quint32>(m_input.data());
            const auto length = qFromLittleEndian<quint32>(m_input.data() + 4);
            if (length > maximumPayload || opcode > 4) { disconnected(); return; }
            if (m_input.size() < 8 + length) break;
            const auto payload = m_input.mid(8, length);
            m_input.remove(0, 8 + length);
            if (opcode == 2 || opcode == 0) { disconnected(); return; }
            if (opcode == 3) {
                if (!write(4, payload)) { disconnected(); return; }
                continue;
            }
            if (opcode == 4) continue;
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(payload, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) { disconnected(); return; }
            const auto message = document.object();
            if (message["evt"] == "ERROR") { disconnected(); return; }
            if (!m_ready && message["cmd"] == "DISPATCH" && message["evt"] == "READY") {
                m_ready = true; m_deadline.stop(); publish();
            } else if (m_ready && !m_nonce.isEmpty() && message["cmd"] == "SET_ACTIVITY"
                       && message["nonce"].toString() == m_nonce) {
                m_nonce.clear(); m_deadline.stop();
                if (!m_update.isActive()) publish();
            }
        }
    }
}
QHash<QString, QString> DiscordPresence::gameIconsFromJson(const QByteArray &json) {
    QHash<QString, QString> icons;
    const QRegularExpression idPattern("\\A[0-9]{16,22}\\z"), hashPattern("\\A(a_)?[a-f0-9]{32}\\z");
    for (const auto &entry : QJsonDocument::fromJson(json).array()) {
        const auto game = entry.toObject();
        const auto id = game["id"].toString(), hash = game["icon"].toString(), name = game["name"].toString().simplified().left(100);
        if (name.isEmpty() || !idPattern.match(id).hasMatch() || !hashPattern.match(hash).hasMatch()) continue;
        const auto url = "https://cdn.discordapp.com/app-icons/" + id + '/' + hash + ".png";
        if (!icons.contains(name)) icons.insert(name, url);
        else if (icons.value(name) != url) icons[name] = {};
    }
    return icons;
}
void DiscordPresence::requestGameIcons() {
    if (m_iconsRequested) return;
    m_iconsRequested = true;
    const QUrl source("https://raw.githubusercontent.com/markterence/discord-quest-completer/53191138e647304dc7dce6a6ace77ae19f8309c3/src/assets/gamelist.json");
    auto *reply = m_network.get(QNetworkRequest(source));
    constexpr qint64 limit = 16 * 1024 * 1024;
    reply->setReadBufferSize(limit + 1);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > limit) reply->abort();
    });
    QTimer::singleShot(10000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() == QNetworkReply::NoError && reply->bytesAvailable() <= limit)
            m_gameIcons = gameIconsFromJson(reply->readAll());
        reply->deleteLater();
        if (!m_update.isActive()) publish();
    });
}
void DiscordPresence::publish() {
    const auto icon = m_gameIcons.value(m_game);
    if (!m_ready || !m_nonce.isEmpty()) return;
    if (m_hasSent && m_sent == m_desired && m_sentGame == m_game && m_sentIcon == icon) return;
    if (m_started && !m_game.isEmpty()) requestGameIcons();
    m_nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString details = m_desired == InstantClip ? "Clipping" : stateText(m_desired);
    if (!m_game.isEmpty()) details += " \"" + m_game + '"';
    QJsonObject assets{{"large_image", "https://raw.githubusercontent.com/Zefth051/Shadowless/1af4b46fece262eac5446581db52d5f8dee14756/linux/app/assets/shadowless-moon.png"},
                       {"large_text", "Shadowless"}};
    if (!icon.isEmpty()) { assets.insert("small_image", icon); assets.insert("small_text", m_game); }
    const QJsonValue activity = m_desired == Idle ? QJsonValue(QJsonValue::Null)
        : QJsonValue(QJsonObject{{"details", details}, {"assets", assets}});
    const QJsonObject command{{"cmd", "SET_ACTIVITY"}, {"nonce", m_nonce},
        {"args", QJsonObject{{"pid", QCoreApplication::applicationPid()}, {"activity", activity}}}};
    if (!write(1, QJsonDocument(command).toJson(QJsonDocument::Compact))) { disconnected(); return; }
    m_sent = m_desired; m_sentGame = m_game; m_sentIcon = icon; m_hasSent = true;
    m_deadline.start(); m_update.start();
}
}
