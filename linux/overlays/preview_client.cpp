#include "preview_client.h"
namespace Shadowless {
PreviewClient::PreviewClient(QObject *parent) : QObject(parent) {
    m_next.setSingleShot(true); m_next.setInterval(250);
    m_timeout.setSingleShot(true); m_timeout.setInterval(1500);
    m_socket.setReadBufferSize(640 * 640 * 4 + 128);
    connect(&m_next, &QTimer::timeout, this, &PreviewClient::request);
    connect(&m_timeout, &QTimer::timeout, this, [this] { retry("Capture preview unavailable."); });
    connect(&m_socket, &QLocalSocket::connected, this, [this] { m_socket.write("FRAME\n"); });
    connect(&m_socket, &QLocalSocket::readyRead, this, &PreviewClient::read);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] { retry("Capture preview unavailable."); });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        if (m_timeout.isActive()) retry("Capture preview unavailable.");
    });
}
void PreviewClient::setSocketPath(const QString &path) {
    if (m_path == path) return;
    m_path = path; m_next.stop(); m_timeout.stop(); m_socket.abort(); m_input.clear();
    emit statusChanged(path.isEmpty() ? "Capture is off · layer editing remains available." : "Waiting for capture preview…");
    if (m_active && !path.isEmpty()) request();
}
void PreviewClient::setActive(bool active) {
    if (m_active == active) return;
    m_active = active; m_next.stop(); m_timeout.stop(); m_socket.abort(); m_input.clear();
    if (active && !m_path.isEmpty()) request();
}
void PreviewClient::request() {
    if (!m_active || m_path.isEmpty()) return;
    m_timeout.stop(); m_socket.abort(); m_input.clear(); m_header = false;
    m_socket.connectToServer(m_path, QIODevice::ReadWrite); m_timeout.start();
}
void PreviewClient::retry(const QString &status) {
    m_timeout.stop(); m_socket.abort(); m_input.clear();
    if (m_active && !m_path.isEmpty()) { emit statusChanged(status); m_next.start(); }
}
void PreviewClient::read() {
    m_input += m_socket.readAll();
    if (!m_header) {
        const auto end = m_input.indexOf('\n');
        if (end < 0) { if (m_input.size() > 100) retry("Invalid capture preview."); return; }
        const auto parts = m_input.left(end).split(' ');
        if (parts.size() != 5 || parts[0] != "SLP1") { retry("Invalid capture preview."); return; }
        const int w = parts[1].toInt(), h = parts[2].toInt(), sw = parts[3].toInt(), sh = parts[4].toInt();
        if (w < 1 || h < 1 || w > 640 || h > 640 || sw < 1 || sh < 1 || sw > 32768 || sh > 32768) {
            retry("Invalid capture preview."); return;
        }
        m_frameSize = QSize(w, h); m_sourceSize = QSize(sw, sh); m_header = true; m_input.remove(0, end + 1);
    }
    const qsizetype bytes = m_frameSize.width() * m_frameSize.height() * 4;
    if (m_input.size() < bytes) return;
    if (m_input.size() != bytes) { retry("Invalid capture preview."); return; }
    const QImage image(reinterpret_cast<const uchar *>(m_input.constData()), m_frameSize.width(), m_frameSize.height(), QImage::Format_RGBA8888);
    const auto copy = image.copy(); m_timeout.stop(); m_socket.abort(); m_input.clear();
    emit frameReady(copy, m_sourceSize); emit statusChanged("Live capture preview");
    if (m_active) m_next.start();
}
}
