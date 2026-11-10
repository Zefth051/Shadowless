#pragma once
#include "layer.h"
#include <QLocalSocket>
#include <QTimer>

namespace Shadowless {
class OverlayClient : public QObject {
    Q_OBJECT
public:
    explicit OverlayClient(QObject *parent = nullptr);
    bool busy() const { return m_busy; }
    void apply(const QString &socketPath, const OverlayScene &scene);
signals:
    void finished(bool success, const QString &error);
private:
    QLocalSocket m_socket;
    QTimer m_timer;
    QByteArray m_reply, m_command;
    bool m_busy = false;
    void finish(bool success, const QString &error);
};
} // namespace Shadowless
