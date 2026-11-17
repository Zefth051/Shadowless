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
protected:
    struct DeviceInfo {
        int leftMinimum = 0, leftMaximum = 255;
        int rightMinimum = 0, rightMaximum = 255;
    };
    // Returned descriptors are owned by this source. Kept virtual for isolated fd fixtures.
    virtual int openDevice(const QString &path);
    virtual bool probeDevice(int fd, DeviceInfo *info);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
}
