#include "input_recorder.h"
#include "input_timeline.h"
#include "layout.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

namespace Shadowless::Input {
namespace {
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}
}

InputRecorder::InputRecorder(InputSource *source, const QString &layoutId, qint64 retentionMs,
                             QObject *parent)
    : QObject(parent), m_source(source), m_layoutId(layoutId), m_retentionMs(retentionMs) {
    Layout layout;
    m_layoutValid = Layout::builtin(layoutId, &layout, nullptr);
    if (m_layoutValid) m_allowlist = layout.allowlist();
}

void InputRecorder::start() {
    if (m_started || !m_source) return;
    m_events.clear();
    m_beforeBuffer.clear();
    m_truncated = false;
    m_latestMs = 0;
    m_started = true;
    m_connection = connect(m_source, &InputSource::inputChanged, this, &InputRecorder::record);
}

void InputRecorder::stop() {
    m_started = false;
    disconnect(m_connection);
    m_connection = {};
}

void InputRecorder::dropOldest() {
    m_beforeBuffer.apply(m_events.front());
    m_events.pop_front();
}

void InputRecorder::evict(qint64 nowMs) {
    if (m_retentionMs <= 0) return;
    const qint64 cutoff = nowMs - m_retentionMs;
    while (!m_events.empty() && m_events.front().tMs < cutoff) dropOldest();
}

void InputRecorder::record(const InputEvent &event) {
    // Queued deliveries can arrive after disconnect().
    if (!m_started || !m_allowlist.contains(event.id)) return;
    m_latestMs = qMax(m_latestMs, event.tMs);
    m_events.push_back(event);
    evict(m_latestMs);
    while (m_events.size() > size_t(InputTimeline::kMaxEvents)) {
        dropOldest();
        m_truncated = true;
    }
}

bool InputRecorder::writeWindow(const QString &clipPath, qint64 windowStartMs,
                                qint64 windowEndMs, QString *error) {
    if (error) error->clear();
    if (!m_layoutValid) return fail(error, QStringLiteral("Unknown input recording layout"));
    if (windowStartMs < 0 || windowEndMs < windowStartMs)
        return fail(error, QStringLiteral("Invalid input recording window"));
    if (clipPath.isEmpty()) return fail(error, QStringLiteral("Empty clip path"));

    evict(qMax(monotonicMs(), m_latestMs));
    InputState held = m_beforeBuffer;
    auto first = m_events.cbegin();
    while (first != m_events.cend() && first->tMs < windowStartMs) held.apply(*first++);
    QStringList ids(held.pressed().begin(), held.pressed().end());
    std::sort(ids.begin(), ids.end());
    QJsonArray events;
    for (const InputId &id : ids) events.append(QJsonArray{0, id, 1});
    for (auto it = first; it != m_events.cend() && it->tMs <= windowEndMs; ++it) {
        const qint64 time = it->tMs - windowStartMs;
        // Keep every file readable by the fixed v1 reader.
        if (time >= 100000000000LL)
            return fail(error, QStringLiteral("Input recording event exceeds the v1 time limit"));
        events.append(QJsonArray{time, it->id, it->down ? 1 : 0});
    }
    bool truncated = m_truncated;
    while (events.size() > InputTimeline::kMaxEvents) {
        // Reserve space for the required initial held state; drop oldest real edges.
        events.removeAt(ids.size());
        truncated = true;
    }
    QJsonObject json{{"schema", 1}, {"layout", m_layoutId},
                     {"durationMs", windowEndMs - windowStartMs}, {"events", events}};
    if (truncated) json.insert("truncated", true);
    const QByteArray bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QSaveFile file(InputTimeline::sidecarPath(clipPath));
    // Never fall back to writing the destination directly if a temporary file fails.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("Cannot create input sidecar temporary file: ") + file.errorString());
    if (file.write(bytes) != bytes.size()) {
        const QString detail = file.errorString();
        file.cancelWriting();
        return fail(error, QStringLiteral("Cannot write input sidecar: ") + detail);
    }
    if (!file.commit())
        return fail(error, QStringLiteral("Cannot atomically replace input sidecar: ") + file.errorString());
    return true;
}
} // namespace Shadowless::Input
