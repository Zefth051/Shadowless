#pragma once
#include "input_types.h"
#include <QObject>

namespace Shadowless::Input {
enum class InputStatus {
    Stopped,          // not started or stopped
    Active,           // at least one readable device
    PermissionDenied, // devices exist but cannot be read (see docs/LINUX_INPUT_OVERLAY.md)
    NoDevices,        // nothing to read right now (hotplug may change this)
    Error             // unexpected failure; statusDetail() explains
};

// Abstract producer of edge events. Implementations: EvdevInputSource (Linux), a hook-based
// source on Windows, FakeInputSource in tests.
// Contract:
//  * start() is opt-in and idempotent; stop() releases every device and is idempotent.
//  * Only ids contained in the allowlist given to start() may ever be emitted.
//  * Nothing is logged or persisted by a source. statusDetail() never contains device names
//    beyond what is needed to guide the user (no key data).
//  * Events are emitted from the thread owning the object, with tMs = monotonicMs().
//  * Stuck keys: when a device disappears or the source stops, up events for held ids are emitted.
class InputSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~InputSource() override = default;
    virtual void start(const QSet<InputId> &allowlist) = 0;
    virtual void stop() = 0;
    InputStatus status() const { return m_status; }
    QString statusDetail() const { return m_detail; }
signals:
    void inputChanged(const Shadowless::Input::InputEvent &event);
    void statusChanged(Shadowless::Input::InputStatus status, const QString &detail);
protected:
    void setStatus(InputStatus status, const QString &detail = {}) {
        if (status == m_status && detail == m_detail) return;
        m_status = status; m_detail = detail;
        emit statusChanged(status, detail);
    }
private:
    InputStatus m_status = InputStatus::Stopped;
    QString m_detail;
};
} // namespace Shadowless::Input
Q_DECLARE_METATYPE(Shadowless::Input::InputEvent)
Q_DECLARE_METATYPE(Shadowless::Input::InputStatus)
