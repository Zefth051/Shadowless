#pragma once
#include "input_types.h"
#include <QJsonObject>
#include <QVector>

namespace Shadowless::Input {
// Reader for the sidecar v1 produced by InputRecorder (docs/LINUX_INPUT_OVERLAY.md). Strict:
// malformed files are rejected whole, and ids outside the recorded layout's allowlist are dropped
// even if present, so a tampered file cannot inject arbitrary identifiers into the editor.
class InputTimeline {
public:
    static constexpr int kMaxEvents = 200000;
    static constexpr qint64 kMaxFileBytes = 8 * 1024 * 1024;
    static QString sidecarPath(const QString &clipPath) { return clipPath + QStringLiteral(".inputs.json"); }
    static bool load(const QString &sidecarFile, InputTimeline *result, QString *error);
    static bool fromJson(const QJsonObject &json, InputTimeline *result, QString *error);

    bool isValid() const { return !m_layout.isEmpty(); }
    QString layoutId() const { return m_layout; }
    qint64 durationMs() const { return m_durationMs; }
    bool truncated() const { return m_truncated; }
    int eventCount() const { return int(m_events.size()); }
    // Keys held at `ms` from the start of the clip. Cheap for monotonic playback and for seeks.
    InputState stateAt(qint64 ms) const;
private:
    QString m_layout;
    qint64 m_durationMs = 0;
    bool m_truncated = false;
    QVector<InputEvent> m_events; // sorted by tMs
    mutable int m_cursor = 0;      // events [0, m_cursor) are applied in m_cached
    mutable qint64 m_cachedMs = -1;
    mutable InputState m_cached;
};

// Which part of the input history a saved clip covers, as monotonicMs() values.
struct ClipWindow { qint64 startMs = 0, endMs = 0; bool valid = false; };
// Replay clip: ends when the save was requested and is `bufferMs` long, or shorter when capture started
// more recently than that.
inline ClipWindow replayClipWindow(qint64 requestMs, qint64 bufferMs, qint64 captureStartMs) {
    if (requestMs <= 0 || captureStartMs <= 0 || requestMs < captureStartMs || bufferMs <= 0) return {};
    const qint64 length = qMin(bufferMs, requestMs - captureStartMs);
    return {requestMs - length, requestMs, true};
}
// Recording: everything since it started. Invalid after a pause, because paused time is cut from the
// video and the events could no longer be aligned with it.
inline ClipWindow recordingClipWindow(qint64 startMs, qint64 endMs, bool pausedOnce) {
    if (startMs <= 0 || endMs < startMs || pausedOnce) return {};
    return {startMs, endMs, true};
}
} // namespace Shadowless::Input
