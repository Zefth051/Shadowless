#pragma once
#include <QImage>
#include <QLocalSocket>
#include <QTimer>
namespace Shadowless {
// One bounded request at a time; disconnecting also withdraws capture demand.
class PreviewClient : public QObject {
    Q_OBJECT
public:
    explicit PreviewClient(QObject *parent = nullptr);
    void setSocketPath(const QString &path);
    void setActive(bool active);
signals:
    void frameReady(const QImage &image, QSize sourceSize);
    void statusChanged(const QString &status);
private:
    QLocalSocket m_socket;
    QTimer m_next, m_timeout;
    QString m_path;
    QByteArray m_input;
    QSize m_frameSize, m_sourceSize;
    bool m_active = false, m_header = false;
    void request();
    void retry(const QString &status);
    void read();
};
}
