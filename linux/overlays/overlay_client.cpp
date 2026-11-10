#include "overlay_client.h"
#include <QJsonDocument>

namespace Shadowless {
OverlayClient::OverlayClient(QObject *parent) : QObject(parent) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] { finish(false, "Overlay update timed out; its applied state is unknown."); });
    connect(&m_socket, &QLocalSocket::connected, this, [this] { m_socket.write(m_command); });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        m_reply += m_socket.readAll();
        if (m_reply.size() > 65536) { finish(false, "Overlay reply is too large."); return; }
        const auto end = m_reply.indexOf('\n'); if (end < 0) return;
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(m_reply.left(end), &parse);
        const auto reply = document.object();
        if (parse.error != QJsonParseError::NoError || !document.isObject() || !reply.value("ok").isBool())
            finish(false, "Invalid overlay acknowledgement.");
        else finish(reply.value("ok").toBool(), reply.value("error").toString());
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] { finish(false, "Overlay connection: " + m_socket.errorString()); });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        if (m_busy) finish(false, "Overlay connection closed before acknowledgement.");
    });
}
void OverlayClient::apply(const QString &socketPath, const OverlayScene &scene) {
    if (m_busy) { emit finished(false, "Another overlay update is pending."); return; }
    m_command = QJsonDocument(scene.toJson()).toJson(QJsonDocument::Compact) + '\n';
    if (m_command.size() > 65536) { emit finished(false, "Overlay update is too large."); return; }
    m_reply.clear(); m_busy = true; m_timer.start(15000); m_socket.connectToServer(socketPath);
}
void OverlayClient::finish(bool success, const QString &error) {
    if (!m_busy) return;
    m_busy = false; m_timer.stop(); m_socket.abort(); emit finished(success, error);
}
} // namespace Shadowless
