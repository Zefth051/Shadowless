#include "input_sidecar_session.h"
#include "input_timeline.h"
#include "layout.h"
#include <QHash>
#include <QDebug>

namespace Shadowless::Input {
InputSidecarSession::InputSidecarSession(InputSource *source, Clock clock, QObject *parent)
    : QObject(parent), m_source(source), m_clock(clock ? std::move(clock) : Clock(monotonicMs)) {}

InputSidecarSession::~InputSidecarSession() { shutdown(); }

QSet<InputId> InputSidecarSession::allowlistFor(const QString &layoutId) {
    static QHash<QString, QSet<InputId>> cache; // GUI thread only
    const auto found = cache.constFind(layoutId);
    if (found != cache.constEnd()) return *found;
    Layout layout; QString error;
    if (!Layout::builtin(layoutId, &layout, &error)) return {};   // never cache a failure: resources may not be ready yet
    return cache.insert(layoutId, layout.allowlist()).value();
}

void InputSidecarSession::update(const State &state) {
    if (m_closed) return;
    const bool recording = state.enabled && state.captureActive && m_source;
    if (!recording) { dropRecorder(); return; } // inactive, but usable again later
    if (!m_recorder || m_layout != state.layout) {
        // A new layout starts with an empty history: events recorded for another layout never leak into it.
        m_recorder = std::make_unique<InputRecorder>(m_source, state.layout, 0);
        m_layout = state.layout;
        m_recorder->start();
    }
    m_bufferMs = state.bufferMs;
    // Replay-only capture needs just the buffer; a recording needs everything since its first frame.
    m_recorder->setRetentionMs(state.recordingOutput ? 0 : state.bufferMs + 10000);
}

QSet<InputId> InputSidecarSession::allowlist() const { return m_recorder ? allowlistFor(m_layout) : QSet<InputId>{}; }

void InputSidecarSession::captureStarted(bool continuousRecording) {
    m_captureStartMs = m_clock();
    if (continuousRecording) { m_recordingStartMs = m_captureStartMs; m_recordingStopMs = 0; m_recordingPausedOnce = false; }
}
void InputSidecarSession::recordingRequested() { m_recordingStopMs = 0; }
void InputSidecarSession::recordingStarted() { m_recordingStartMs = m_clock(); m_recordingStopMs = 0; m_recordingPausedOnce = false; }
void InputSidecarSession::recordingPaused() { m_recordingPausedOnce = true; }
void InputSidecarSession::recordingStopRequested() { m_recordingStopMs = m_clock(); }
void InputSidecarSession::replaySaveRequested(qint64 bufferMs) {
    m_clipRequestMs = m_clock();
    m_clipBufferMs = bufferMs >= 0 ? bufferMs : m_bufferMs;   // the buffer in force when the save was asked for
}

InputSidecarSession::Outcome InputSidecarSession::writeClip(const QString &clipPath, bool recording, QString *error) {
    if (error) error->clear();
    if (!m_recorder) return Outcome::Skipped;
    const qint64 now = m_clock();
    const auto window = recording
        ? recordingClipWindow(m_recordingStartMs, m_recordingStopMs > 0 ? m_recordingStopMs : now, m_recordingPausedOnce)
        : replayClipWindow(m_clipRequestMs > 0 ? m_clipRequestMs : now, m_clipRequestMs > 0 ? m_clipBufferMs : m_bufferMs, m_captureStartMs);
    if (!window.valid) return Outcome::Skipped;
    QString failure;
    if (m_recorder->writeWindow(clipPath, window.startMs, window.endMs, &failure)) return Outcome::Written;
    if (error) *error = failure;
    qWarning().noquote() << "Input events were not saved:" << failure;
    return Outcome::Failed;
}

void InputSidecarSession::dropRecorder() {
    if (m_recorder) m_recorder->stop();
    m_recorder.reset();
    m_layout.clear();
}
void InputSidecarSession::shutdown() {
    dropRecorder();
    m_closed = true;
}
} // namespace Shadowless::Input
