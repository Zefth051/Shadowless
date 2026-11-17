#pragma once

#include <QString>
#include <QVector>
#include <functional>

namespace Shadowless::Camera {

enum class Kind { Physical, VirtualLoopback, Unknown };
enum class Usability {
    LikelyUsable, NodeMissing, PermissionDenied, SecondaryNode, NoProducerUnknown, Unknown
};

struct CameraDevice {
    QString id;
    QString node;
    QString name;
    QString driver;
    QString bus = QStringLiteral("unknown");
    int index = -1;
    Kind kind = Kind::Unknown;
    QString bridgeHint;
    Usability usability = Usability::Unknown;
    QString reason;
    bool detected = true;
    bool likelyUsable() const { return detected && usability == Usability::LikelyUsable; }
};

struct Probe {
    std::function<bool(const QString &)> isCharDevice;
    std::function<bool(const QString &)> canReadWrite;
    std::function<QString(const QString &)> resolveLink;
};

// Discovery only: never opens device nodes, captures, or changes permissions.
class CameraCatalog {
public:
    using Probe = Shadowless::Camera::Probe;
    explicit CameraCatalog(QString sysfsRoot = QStringLiteral("/sys/class/video4linux"),
                           QString devRoot = QStringLiteral("/dev"), Probe probe = {});
    QVector<CameraDevice> scan() const;

private:
    QString m_sysfsRoot;
    QString m_devRoot;
    Probe m_probe;
};

} // namespace Shadowless::Camera
