#pragma once
#include "input_recorder.h"
#include <QPointer>
#include <functional>
#include <memory>

namespace Shadowless::Input {
// Owns the sidecar (`<clip>.inputs.json`) lifecycle for one capture: when the recorder exists, how the timing marks
// of the capture map to the window of a saved clip, and when a clip must NOT get a sidecar. The controller only reports
// what happens (capture started, save requested, recording paused...) and asks for the file; all rules live here so
// they can be tested with a fake clock and a fake input source.
class InputSidecarSession : public QObject {
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;
    // What the controller knows right now. Passing the same state again is a no-op.
    struct State {
        bool enabled = false;         // saved events switched on and an overlay layout chosen
        QString layout;               // built-in layout id
        bool captureActive = false;   // the capture engine process is running (also while it finishes saving)
        bool recordingOutput = false; // continuous recording or a recording next to the replay buffer
        qint64 bufferMs = 0;          // replay buffer length
    };
    enum class Outcome { Written, Skipped, Failed };

    explicit InputSidecarSession(InputSource *source, Clock clock = monotonicMs, QObject *parent = nullptr);
    ~InputSidecarSession() override;

    void update(const State &state);
    bool active() const { return bool(m_recorder); }
    // Ids the shared source must emit for this session; empty while inactive.
    QSet<InputId> allowlist() const;
    // Allowlist of a built-in layout, parsed once. Empty for unknown ids.
    static QSet<InputId> allowlistFor(const QString &layoutId);

    // Timing marks, reported as they happen.
    void captureStarted(bool continuousRecording);
    void recordingRequested();   // a recording is about to start next to the replay buffer
    void recordingStarted();     // its first frame
    void recordingPaused();      // paused time is cut from the video: the recording can no longer be aligned
    void recordingStopRequested();
    void replaySaveRequested();

    // Writes the sidecar for a saved clip. Skipped: nothing to write (inactive, paused or unknown timing).
    // Failed: the file could not be written; the clip itself is never affected.
    Outcome writeClip(const QString &clipPath, bool recording, QString *error = nullptr);
    // Stops recording and drops the history. Idempotent; later calls to update() stay inactive.
    void shutdown();

private:
    void dropRecorder();
    QPointer<InputSource> m_source;
    Clock m_clock;
    std::unique_ptr<InputRecorder> m_recorder;
    QString m_layout;
    qint64 m_bufferMs = 0;
    qint64 m_captureStartMs = 0, m_clipRequestMs = 0, m_recordingStartMs = 0, m_recordingStopMs = 0;
    bool m_recordingPausedOnce = false, m_closed = false;
};
} // namespace Shadowless::Input
