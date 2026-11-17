#pragma once
// Test double for the InputSource contract. Header-only so it needs no moc.
#include "input_source.h"

namespace Shadowless::Input {
class FakeInputSource : public InputSource {
public:
    using InputSource::InputSource;
    void start(const QSet<InputId> &allowlist) override {
        m_allow = allowlist; m_started = true; setStatus(InputStatus::Active);
    }
    void stop() override {
        for (const InputId &id : QList<InputId>(m_held.begin(), m_held.end())) inject(id, false);
        m_started = false; setStatus(InputStatus::Stopped);
    }
    // Behaves like a real source: drops ids outside the allowlist. Returns whether it was emitted.
    bool inject(const InputId &id, bool down, qint64 tMs = monotonicMs()) {
        if (!m_started || !m_allow.contains(id)) return false;
        if (down) m_held.insert(id); else m_held.remove(id);
        emit inputChanged(InputEvent{id, down, tMs});
        return true;
    }
    void fail(InputStatus status, const QString &detail) { setStatus(status, detail); }
private:
    QSet<InputId> m_allow, m_held;
    bool m_started = false;
};
} // namespace Shadowless::Input
