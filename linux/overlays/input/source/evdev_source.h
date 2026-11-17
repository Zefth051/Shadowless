#pragma once
#include "../core/input_source.h"
#include <memory>

namespace Shadowless::Input {
class EvdevInputSource : public InputSource {
    Q_OBJECT
public:
    explicit EvdevInputSource(QObject *parent = nullptr);
    explicit EvdevInputSource(const QString &deviceDirectory, QObject *parent = nullptr);
    ~EvdevInputSource() override;
    void start(const QSet<InputId> &allowlist) override;
    void stop() override;
    // Forgets devices that could not be opened and scans again now. For user-driven retries (for example when the
    // overlay editor opens): a permission change that sends no inotify attribute event is picked up this way.
    void retryInaccessible();
protected:
    // Configure before start(); maxAttempts includes the initial open.
    void setRetryPolicy(int initialMs, int maxMs, int maxAttempts);
    struct DeviceInfo {
        int leftMinimum = 0, leftMaximum = 255;
        int rightMinimum = 0, rightMaximum = 255;
        bool probeError = false; // probing failed for a reason other than "not an input device": retry with backoff
    };
    // Returned descriptors are owned by this source. Kept virtual for isolated fd fixtures.
    virtual int openDevice(const QString &path);
    virtual bool probeDevice(int fd, DeviceInfo *info);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
}
