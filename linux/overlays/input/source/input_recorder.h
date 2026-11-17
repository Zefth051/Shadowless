#pragma once
#include "input_source.h"
#include <QPointer>
#include <deque>

namespace Shadowless::Input {
class InputRecorder : public QObject {
    Q_OBJECT
public:
    InputRecorder(InputSource *source, const QString &layoutId, qint64 retentionMs,
                  QObject *parent = nullptr);
    // Starts a fresh recording session. Does not start or stop the shared source.
    void start();
    void stop();
    bool writeWindow(const QString &clipPath, qint64 windowStartMs, qint64 windowEndMs,
                     QString *error);
private:
    void record(const InputEvent &event);
    void evict(qint64 nowMs);
    void dropOldest();

    QPointer<InputSource> m_source;
    QString m_layoutId;
    qint64 m_retentionMs;
    QSet<InputId> m_allowlist;
    bool m_layoutValid = false;
    bool m_started = false;
    bool m_truncated = false;
    qint64 m_latestMs = 0;
    QMetaObject::Connection m_connection;
    std::deque<InputEvent> m_events;
    InputState m_beforeBuffer; // Held state just before the retained events.
};
} // namespace Shadowless::Input
