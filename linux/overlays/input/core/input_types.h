#pragma once
// Input overlay contract (v1). Shared vocabulary between input sources (evdev on Linux,
// low-level hooks on Windows), the renderer, the sidecar recorder and the editor overlay.
// Identifiers are stable strings so layouts and sidecar files are portable across platforms.
//
//   key.<name>      key.w key.lshift key.space key.up key.f5 ...
//   mouse.<name>    mouse.left mouse.right mouse.middle mouse.back mouse.forward
//                   mouse.wheelup mouse.wheeldown (momentary: down immediately followed by up)
//   pad.<name>      pad.south/east/west/north pad.l1 pad.r1 pad.l2 pad.r2 pad.l3 pad.r3
//                   pad.select pad.start pad.mode pad.dpad.up/down/left/right
//
// Privacy: only identifiers in an explicit allowlist ever leave a source. No text, scan codes,
// device names or wall-clock times are part of this contract.
#include <QSet>
#include <QString>
#include <QStringList>
#include <chrono>

namespace Shadowless::Input {
using InputId = QString;

struct InputEvent {
    InputId id;
    bool down = false;
    qint64 tMs = 0; // monotonicMs() at the moment of the edge
};

// Common clock for sources and recorders. Millisecond resolution is deliberate.
inline qint64 monotonicMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Built-in layout ids in dropdown order. "none" (no overlay) is not a layout.
inline QStringList builtinLayoutIds() { return {"qwerty", "qwerty-full", "arrows", "azerty", "dualsense"}; }

inline bool isKnownPrefix(const InputId &id) {
    return id.startsWith(QLatin1String("key.")) || id.startsWith(QLatin1String("mouse."))
        || id.startsWith(QLatin1String("pad."));
}

// Which ids are currently held. Ignores ids outside the optional allowlist.
class InputState {
public:
    void apply(const InputEvent &event) {
        if (event.id.isEmpty()) return;
        if (event.down) m_down.insert(event.id); else m_down.remove(event.id);
    }
    bool isDown(const InputId &id) const { return m_down.contains(id); }
    const QSet<InputId> &pressed() const { return m_down; }
    void clear() { m_down.clear(); }
    bool operator==(const InputState &o) const { return m_down == o.m_down; }
private:
    QSet<InputId> m_down;
};
} // namespace Shadowless::Input
