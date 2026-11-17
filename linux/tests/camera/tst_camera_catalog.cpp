#include "../../capture/camera/camera_catalog.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>
#include <cerrno>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Shadowless::Camera;

namespace {
struct Tree {
    QTemporaryDir temp;
    QString sys = temp.filePath("sys");
    QString dev = temp.filePath("dev");
    Tree() { QDir().mkpath(sys); QDir().mkpath(dev); }
    static bool write(const QString &path, const QByteArray &value) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(value) == value.size();
    }
    bool add(const QString &id, const QByteArray &name, const QByteArray &index,
             const QString &device = "devices/usb1/camera", const QString &driver = "uvcvideo") {
        const QString entry = sys + '/' + id;
        const QString target = temp.filePath(device);
        const QString driverTarget = temp.filePath("drivers/" + driver);
        return QDir().mkpath(entry) && QDir().mkpath(target) && QDir().mkpath(driverTarget)
            && write(entry + "/name", name) && write(entry + "/index", index)
            && QFile::link(target, entry + "/device")
            && (QFileInfo::exists(target + "/driver") || QFile::link(driverTarget, target + "/driver"))
            && write(dev + '/' + id, {});
    }
    Probe probe() const {
        return {[](const QString &path) { return QFileInfo::exists(path); },
                [](const QString &) { return true; },
                [](const QString &path) { return QFileInfo(path).canonicalFilePath(); }};
    }
    QVector<CameraDevice> scan() const { return CameraCatalog(sys, dev, probe()).scan(); }
};
}

class CameraCatalogTests : public QObject {
    Q_OBJECT
private slots:
    void uvcPair() {
        Tree t;
        QVERIFY(t.add("video1", "Metadata", "1\n"));
        QVERIFY(t.add("video0", "Webcam\n", "0\n"));
        const auto devices = t.scan();
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].id, "video0");
        QCOMPARE(devices[0].node, t.dev + "/video0");
        QCOMPARE(devices[0].name, "Webcam");
        QCOMPARE(devices[0].driver, "uvcvideo");
        QCOMPARE(devices[0].bus, "usb");
        QCOMPARE(devices[0].kind, Kind::Physical);
        QCOMPARE(devices[0].usability, Usability::LikelyUsable);
        QVERIFY(devices[0].detected);
        QVERIFY(devices[0].likelyUsable());
        QCOMPARE(devices[1].usability, Usability::SecondaryNode);
        QVERIFY(!devices[1].likelyUsable());
        QVERIFY(!devices[1].reason.isEmpty());
    }
    void permissionAndMissingNode() {
        Tree t;
        QVERIFY(t.add("video0", "Camera", "0"));
        Probe p = t.probe();
        p.canReadWrite = [](const QString &) { return false; };
        const auto denied = CameraCatalog(t.sys, t.dev, p).scan();
        QCOMPARE(denied.size(), 1);
        QCOMPARE(denied[0].usability, Usability::PermissionDenied);
        QVERIFY(!denied[0].likelyUsable());
        QVERIFY(QFile::remove(t.dev + "/video0"));
        const auto missing = t.scan();
        QCOMPARE(missing.size(), 1);
        QCOMPARE(missing[0].usability, Usability::NodeMissing);
        QVERIFY(missing[0].detected);
    }
    void bridges_data() {
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<QString>("driver");
        QTest::addColumn<QString>("hint");
        QTest::newRow("droidcam") << QByteArray("DroidCam") << QString("v4l2loopback") << QString("DroidCam");
        QTest::newRow("obs-name") << QByteArray("oBs ViRtUaL CaMeRa") << QString("other") << QString("OBS Virtual Camera");
        QTest::newRow("scrcpy-name") << QByteArray("scrcpy camera") << QString("other") << QString("scrcpy");
        QTest::newRow("generic-driver") << QByteArray("Camera") << QString("v4l2loopback") << QString("generic v4l2loopback");
        QTest::newRow("generic-name") << QByteArray("V4L2Loopback Camera") << QString("other") << QString("generic v4l2loopback");
    }
    void bridges() {
        QFETCH(QByteArray, name);
        QFETCH(QString, driver);
        QFETCH(QString, hint);
        Tree t;
        QVERIFY(t.add("video0", name, "0", "devices/virtual/video", driver));
        const auto devices = t.scan();
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices[0].kind, Kind::VirtualLoopback);
        QCOMPARE(devices[0].bus, "virtual");
        QCOMPARE(devices[0].bridgeHint, hint);
        QCOMPARE(devices[0].usability, Usability::NoProducerUnknown);
        QVERIFY(!devices[0].likelyUsable());
        QVERIFY(devices[0].reason.contains("producer", Qt::CaseInsensitive));
        QVERIFY(devices[0].reason.contains("opening", Qt::CaseInsensitive));
    }
    void garbageAndSorting() {
        Tree t;
        for (const QString id : {"video10", "video2", "videoX", "video-1", "video3junk", "video"})
            QVERIFY(t.add(id, "Camera", "0"));
        QVERIFY(Tree::write(t.sys + "/video9", "not an entry directory"));
        const auto devices = t.scan();
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].id, "video2");
        QCOMPARE(devices[1].id, "video10");
    }
    void malformedAndMissingAttributes() {
        Tree t;
        QVERIFY(t.add("video0", "", "garbage"));
        QVERIFY(t.add("video1", "Camera", "-1", "devices/platform/camera"));
        QVERIFY(t.add("video2", "Camera", "999999999999999999999", "devices/pci0000:00/camera"));
        QVERIFY(QFile::remove(t.sys + "/video0/name"));
        QVERIFY(QFile::remove(t.sys + "/video0/device"));
        auto devices = t.scan();
        QCOMPARE(devices.size(), 3);
        QCOMPARE(devices[0].index, -1);
        QCOMPARE(devices[0].kind, Kind::Unknown);
        QCOMPARE(devices[0].bus, "unknown");
        QCOMPARE(devices[0].usability, Usability::Unknown);
        QCOMPARE(devices[1].index, -1);
        QCOMPARE(devices[1].bus, "platform");
        QCOMPARE(devices[2].index, -1);
        QCOMPARE(devices[2].bus, "pci");
        QVERIFY(QFile::remove(t.sys + "/video0/index"));
        QCOMPARE(t.scan()[0].index, -1);
    }
    void sanitizesNames() {
        Tree t;
        QVERIFY(t.add("video0", QByteArray(" \tCam\x01\x7f\n "), "0"));
        QVERIFY(t.add("video1", QByteArray(100, 'A'), "0"));
        const auto devices = t.scan();
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].name, "Cam");
        QCOMPARE(devices[1].name, QString(80, 'A'));
    }
    void independentSecondaryAndDriverFallback() {
        Tree t;
        QVERIFY(t.add("video0", "Camera", "0", "devices/usb/camera0"));
        QVERIFY(t.add("video1", "Camera", "1", "devices/usb/camera1"));
        QVERIFY(QFile::remove(t.sys + "/video1/device/driver"));
        QVERIFY(QFile::link(t.temp.filePath("drivers/uvcvideo"), t.sys + "/video1/driver"));
        const auto devices = t.scan();
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[1].usability, Usability::LikelyUsable);
        QCOMPARE(devices[1].driver, "uvcvideo");
        QCOMPARE(devices[1].bus, "usb");
    }
    void sysfsClassSymlink() {
        Tree t;
        QVERIFY(t.add("video0", "Camera", "0"));
        const QString backing = t.temp.filePath("backing-video0");
        QVERIFY(QDir().rename(t.sys + "/video0", backing));
        QVERIFY(QFile::link(backing, t.sys + "/video0"));
        const auto devices = CameraCatalog(t.sys, t.dev, {t.probe().isCharDevice,
                                                         t.probe().canReadWrite, {}}).scan();
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices[0].driver, "uvcvideo");
        QCOMPARE(devices[0].bus, "usb");
        QVERIFY(devices[0].likelyUsable());
    }
    void emptyAndMissingRoot() {
        Tree t;
        QVERIFY(t.scan().isEmpty());
        QVERIFY(CameraCatalog(t.sys + "/absent", t.dev).scan().isEmpty());
    }
    void deviceNodesAreNeverOpened() {
        Tree t;
        QVERIFY(t.add("video0", "FIFO", "0"));
        QVERIFY(t.add("video1", "Unreadable", "0"));
        QVERIFY(QFile::remove(t.dev + "/video0"));
        QCOMPARE(::mkfifo(QFile::encodeName(t.dev + "/video0").constData(), 0600), 0);
        QVERIFY(QFile::setPermissions(t.dev + "/video1", {}));
        const int watcher = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        QVERIFY(watcher >= 0);
        const int watch = ::inotify_add_watch(watcher, QFile::encodeName(t.dev).constData(), IN_OPEN);
        if (watch < 0) {
            ::close(watcher);
            QFAIL("Cannot watch fake device nodes for opens");
        }
        QElapsedTimer timer;
        timer.start();
        const auto devices = CameraCatalog(t.sys, t.dev).scan();
        // Injected char-device checks must not lead to any open either. Opening
        // the FIFO for reading would hang; opening an ordinary file emits IN_OPEN.
        const auto pretendChars = t.scan();
        char events[4096];
        const ssize_t bytes = ::read(watcher, events, sizeof(events));
        const int readError = errno;
        ::close(watcher);
        QCOMPARE(bytes, ssize_t(-1));
        QCOMPARE(readError, EAGAIN);
        QCOMPARE(pretendChars.size(), 2);
        QVERIFY(pretendChars[0].likelyUsable());
        QVERIFY(pretendChars[1].likelyUsable());
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].usability, Usability::NodeMissing);
        QCOMPARE(devices[1].usability, Usability::NodeMissing);
        QVERIFY(timer.elapsed() < 2000);
        // Also exercise access(2) without replacing the character-device predicate.
        QVERIFY(t.add("video2", "Char device", "0"));
        QVERIFY(QFile::remove(t.dev + "/video2"));
        QVERIFY(QFile::link("/dev/null", t.dev + "/video2"));
        const auto withChar = CameraCatalog(t.sys, t.dev).scan();
        QCOMPARE(withChar.size(), 3);
        QCOMPARE(withChar[2].usability, Usability::LikelyUsable);
    }
    void realSysfsReadOnlySanity() {
        const auto devices = CameraCatalog().scan();
        for (const auto &device : devices) {
            QVERIFY(device.detected);
            QVERIFY(QRegularExpression("^video[0-9]+$").match(device.id).hasMatch());
            QCOMPARE(device.node, "/dev/" + device.id);
            QVERIFY(device.name.size() <= 80);
            for (const QChar c : device.name)
                QVERIFY(c.category() != QChar::Other_Control && c.category() != QChar::Other_Format);
            QVERIFY(!device.reason.isEmpty());
            QCOMPARE(device.likelyUsable(), device.usability == Usability::LikelyUsable);
            if (device.kind == Kind::VirtualLoopback)
                QVERIFY(!device.likelyUsable());
        }
    }
};
QTEST_GUILESS_MAIN(CameraCatalogTests)
#include "tst_camera_catalog.moc"
