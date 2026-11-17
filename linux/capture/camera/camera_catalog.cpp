#include "camera_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace Shadowless::Camera {
namespace {
QString attribute(const QString &path)
{
    // sysfs attributes are small regular files; reject FIFOs and bound malformed input.
    if (!QFileInfo(path).isFile())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.read(4096));
}

QString sanitizedName(const QString &value)
{
    QString result;
    for (const QChar c : value) {
        if (c.category() != QChar::Other_Control && c.category() != QChar::Other_Format)
            result += c;
    }
    result = result.trimmed().left(80);
    if (!result.isEmpty() && result.back().isHighSurrogate())
        result.chop(1);
    return result;
}

QString busFor(const QString &target)
{
    static const QRegularExpression usb(QStringLiteral("/usb(?:[0-9]+)?(?:/|$)"));
    if (usb.match(target).hasMatch())
        return QStringLiteral("usb");
    if (target.contains(QStringLiteral("/pci")))
        return QStringLiteral("pci");
    if (target.contains(QStringLiteral("/virtual/")))
        return QStringLiteral("virtual");
    if (target.contains(QStringLiteral("/platform/")))
        return QStringLiteral("platform");
    return QStringLiteral("unknown");
}

QString bridgeFor(const QString &name, const QString &driver)
{
    if (name.contains(QStringLiteral("droidcam"), Qt::CaseInsensitive))
        return QStringLiteral("DroidCam");
    if (name.contains(QStringLiteral("obs virtual camera"), Qt::CaseInsensitive))
        return QStringLiteral("OBS Virtual Camera");
    if (name.contains(QStringLiteral("scrcpy"), Qt::CaseInsensitive))
        return QStringLiteral("scrcpy");
    if (driver.compare(QStringLiteral("v4l2loopback"), Qt::CaseInsensitive) == 0
        || name.contains(QStringLiteral("v4l2loopback"), Qt::CaseInsensitive))
        return QStringLiteral("generic v4l2loopback");
    return {};
}

QString numericSuffix(const QString &id)
{
    QString number = id.mid(5);
    while (number.size() > 1 && number.front() == QLatin1Char('0'))
        number.remove(0, 1);
    return number;
}
} // namespace

CameraCatalog::CameraCatalog(QString sysfsRoot, QString devRoot, Probe probe)
    : m_sysfsRoot(std::move(sysfsRoot)), m_devRoot(std::move(devRoot)), m_probe(std::move(probe))
{
    if (!m_probe.isCharDevice) {
        m_probe.isCharDevice = [](const QString &path) {
            struct stat info {};
            return ::stat(QFile::encodeName(path).constData(), &info) == 0 && S_ISCHR(info.st_mode);
        };
    }
    if (!m_probe.canReadWrite) {
        m_probe.canReadWrite = [](const QString &path) {
            return ::access(QFile::encodeName(path).constData(), R_OK | W_OK) == 0;
        };
    }
    if (!m_probe.resolveLink) {
        m_probe.resolveLink = [](const QString &path) {
            const QFileInfo info(path);
            return info.isSymLink() ? info.canonicalFilePath() : QString();
        };
    }
}

QVector<CameraDevice> CameraCatalog::scan() const
{
    QVector<CameraDevice> result;
    QVector<QString> targets;
    QSet<QString> primaryTargets;
    static const QRegularExpression entryPattern(QStringLiteral("^video[0-9]+$"));
    static const QRegularExpression indexPattern(QStringLiteral("^[0-9]+$"));
    const QDir sysfs(m_sysfsRoot);
    const QDir dev(m_devRoot);
    const auto entries = sysfs.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &id : entries) {
        if (!entryPattern.match(id).hasMatch())
            continue;
        const QString entry = sysfs.filePath(id);
        CameraDevice camera;
        camera.id = id;
        camera.node = dev.filePath(id);
        camera.name = sanitizedName(attribute(entry + QStringLiteral("/name")));
        const QString indexText = attribute(entry + QStringLiteral("/index")).trimmed();
        bool indexOk = false;
        if (indexPattern.match(indexText).hasMatch()) {
            const int index = indexText.toInt(&indexOk);
            if (indexOk)
                camera.index = index;
        }
        const QString target = m_probe.resolveLink(entry + QStringLiteral("/device"));
        camera.bus = busFor(target);
        QString driverTarget = m_probe.resolveLink(entry + QStringLiteral("/device/driver"));
        if (driverTarget.isEmpty())
            driverTarget = m_probe.resolveLink(entry + QStringLiteral("/driver"));
        camera.driver = QFileInfo(driverTarget).fileName();
        camera.bridgeHint = bridgeFor(camera.name, camera.driver);
        if (!camera.bridgeHint.isEmpty())
            camera.kind = Kind::VirtualLoopback;
        else if (camera.driver == QStringLiteral("uvcvideo") || camera.bus == QStringLiteral("usb")
                 || camera.bus == QStringLiteral("pci") || camera.bus == QStringLiteral("platform"))
            camera.kind = Kind::Physical;
        if (camera.index == 0 && !target.isEmpty())
            primaryTargets.insert(target);
        targets.append(target);
        result.append(std::move(camera));
    }

    for (qsizetype i = 0; i < result.size(); ++i) {
        auto &camera = result[i];
        if (!m_probe.isCharDevice(camera.node)) {
            camera.usability = Usability::NodeMissing;
            camera.reason = QStringLiteral("The device node is missing or is not a character device.");
        } else if (!m_probe.canReadWrite(camera.node)) {
            camera.usability = Usability::PermissionDenied;
            camera.reason = QStringLiteral("Read and write access is unavailable for the current user.");
        } else if (camera.index > 0 && !targets[i].isEmpty() && primaryTargets.contains(targets[i])) {
            camera.usability = Usability::SecondaryNode;
            camera.reason = QStringLiteral("Secondary node: an index-0 sibling exists for this device (metadata heuristic).");
        } else if (camera.kind == Kind::VirtualLoopback) {
            camera.usability = Usability::NoProducerUnknown;
            camera.reason = QStringLiteral("A loopback producer cannot be verified without opening the device; usability is unknown.");
        } else if (camera.index < 0) {
            camera.usability = Usability::Unknown;
            camera.reason = QStringLiteral("The sysfs index is missing or malformed; usability is unknown.");
        } else {
            camera.usability = Usability::LikelyUsable;
            camera.reason = QStringLiteral("Character device with read and write access; capture support is unverified without opening.");
        }
    }
    std::sort(result.begin(), result.end(), [](const CameraDevice &left, const CameraDevice &right) {
        const QString a = numericSuffix(left.id);
        const QString b = numericSuffix(right.id);
        if (a.size() != b.size())
            return a.size() < b.size();
        if (a != b)
            return a < b;
        return left.id < right.id;
    });
    return result;
}
} // namespace Shadowless::Camera
