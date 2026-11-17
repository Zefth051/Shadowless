#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include "evdev_source.h"
#include "evdev_keymap.h"
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <cstdarg>
#include <cstring>

using namespace Shadowless::Input;

// Pipe fixtures cannot implement evdev ioctls; emulate one kernel snapshot response.
static int snapshotFd = -1;
static QString countedPath;
static int identityChecks = 0;
// Count per-node scan work without replacing the real identity lookup.
extern "C" int __real_stat(const char *path, struct stat *result);
extern "C" int __wrap_stat(const char *path, struct stat *result) {
    if (!countedPath.isEmpty() && QFile::decodeName(path) == countedPath) ++identityChecks;
    return __real_stat(path, result);
}
extern "C" int __real_ioctl(int fd, unsigned long request, ...);
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    va_list args; va_start(args, request); void *data = va_arg(args, void *); va_end(args);
    if (fd == snapshotFd && _IOC_TYPE(request) == 'E' && _IOC_NR(request) == 0x18) {
        const auto size = _IOC_SIZE(request);
        std::memset(data, 0, size);
        static_cast<unsigned char *>(data)[KEY_W / 8] |= 1 << (KEY_W % 8);
        return int(size); // EVIOCGKEY returns the number of bytes copied, not zero.
    }
    return __real_ioctl(fd, request, data);
}

class TestSource : public EvdevInputSource {
public:
    explicit TestSource(const QString &path) : EvdevInputSource(path) {}
    using EvdevInputSource::setRetryPolicy;
    QHash<QString, int> readers;
    QList<qint64> attemptTimes;
    QList<int> opened;
    bool denied = false, holdWOnOpen = false, rejected = false, failConfiguration = false;
    QHash<QString, int> attempts;
    int openError = 0;
    void triggerRange(int minimum, int maximum) {
        info.leftMinimum = info.rightMinimum = minimum;
        info.leftMaximum = info.rightMaximum = maximum;
    }
protected:
    int openDevice(const QString &path) override {
        ++attempts[QFileInfo(path).fileName()];
        attemptTimes.append(monotonicMs());
        if (denied || openError) { errno = openError ? openError : EACCES; return -1; }
        const int fd = readers.value(QFileInfo(path).fileName(), -1);
        if (fd < 0) { errno = ENOENT; return -1; }
        const int owned = dup(fd);
        if (owned >= 0) opened.append(owned);
        if (owned >= 0 && holdWOnOpen) snapshotFd = owned; // the fixture reports W as held from the first query
        if (owned >= 0 && failConfiguration) close(owned); // Make the source's fcntl fail.
        return owned;
    }
    DeviceInfo info;
    bool probeDevice(int, DeviceInfo *result) override {
        *result = info;
        return !rejected;
    }
};

struct Pipe {
    int fds[2]{-1, -1};
    Pipe() { if (pipe2(fds, O_NONBLOCK | O_CLOEXEC)) qFatal("pipe2 failed"); }
    ~Pipe() { for (int fd : fds) if (fd >= 0) close(fd); }
    void send(unsigned short type, unsigned short code, int value) {
        input_event e{}; e.type = type; e.code = code; e.value = value;
        if (write(fds[1], &e, sizeof(e)) != sizeof(e)) qFatal("write failed");
    }
};
static void touch(const QString &path) { QFile f(path); if (!f.open(QIODevice::WriteOnly)) qFatal("touch failed"); }
static InputEvent eventAt(const QSignalSpy &spy, int i) { return qvariant_cast<InputEvent>(spy.at(i).at(0)); }

class EvdevTests : public QObject {
    Q_OBJECT
private slots:
    void cleanup() { snapshotFd = -1; countedPath.clear(); identityChecks = 0; }
    void rejectedNodesStayCached() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.rejected = true; source.readers.insert("event0", pipe.fds[0]);
        touch(dir.filePath("event0")); source.start({"key.w"});
        QCOMPARE(source.attempts.value("event0"), 1);
        for (int batch = 0; batch < 5; ++batch) {
            for (int i = 0; i < 100; ++i) touch(dir.filePath(QString("noise%1").arg(i)));
            QVERIFY(QFile::setPermissions(dir.filePath("event0"), QFile::ReadOwner | QFile::WriteOwner));
            QTest::qWait(40);
        }
        QCOMPARE(source.attempts.value("event0"), 1);
        QCOMPARE(source.status(), InputStatus::NoDevices);
        // Renaming a replacement over the node changes its inode without IN_DELETE.
        touch(dir.filePath("replacement"));
        const auto old = dir.filePath("event0").toLocal8Bit();
        const auto replacement = dir.filePath("replacement").toLocal8Bit();
        QCOMPARE(::rename(replacement.constData(), old.constData()), 0);
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 2, 200);
        QVERIFY(QFile::remove(dir.filePath("event0"))); touch(dir.filePath("event0"));
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 3, 200);
        source.stop(); source.start({"key.w"});
        QCOMPARE(source.attempts.value("event0"), 4);
    }
    void unrelatedHotplugDoesNotRetryDeniedNode() {
        QTemporaryDir dir; TestSource source(dir.path());
        touch(dir.filePath("event0")); source.denied = true; source.start({"key.w"});
        for (int i = 0; i < 5; ++i) {
            touch(dir.filePath(QString("noise%1").arg(i))); QTest::qWait(40);
        }
        QCOMPARE(source.attempts.value("event0"), 1);
        QCOMPARE(source.status(), InputStatus::PermissionDenied);
    }
    void boundedRetries_data() {
        QTest::addColumn<int>("error");
        QTest::addColumn<InputStatus>("expectedStatus");
        QTest::newRow("access") << EACCES << InputStatus::PermissionDenied;
        QTest::newRow("permission") << EPERM << InputStatus::PermissionDenied;
        QTest::newRow("io") << EIO << InputStatus::Error;
        QTest::newRow("other") << EMFILE << InputStatus::Error;
        QTest::newRow("gone") << ENODEV << InputStatus::NoDevices;
        QTest::newRow("missing") << ENOENT << InputStatus::NoDevices;
    }
    void boundedRetries() {
        QFETCH(int, error); QFETCH(InputStatus, expectedStatus);
        QTemporaryDir dir; TestSource source(dir.path());
        source.setRetryPolicy(10, 20, 6); source.openError = error;
        touch(dir.filePath("event0")); source.start({"key.w"});
        QCOMPARE(source.attempts.value("event0"), 1);
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 6, 200);
        QCOMPARE(source.status(), expectedStatus);
        QCOMPARE(source.attemptTimes.size(), 6);
        // The first delay doubles once, then remains capped at 20 ms.
        for (int i = 1; i < 6; ++i) {
            const auto interval = source.attemptTimes[i] - source.attemptTimes[i-1];
            QVERIFY(interval >= (i == 1 ? 10 : 20));
            QVERIFY(interval < 100);
        }
        QVERIFY(source.findChildren<QTimer *>().size() == 1); // Only the inactive debounce remains.
        QTest::qWait(120); QCOMPARE(source.attempts.value("event0"), 6);
        touch(dir.filePath("noise")); QTest::qWait(60);
        QCOMPARE(source.attempts.value("event0"), 6); QCOMPARE(source.status(), expectedStatus);
        QVERIFY(QFile::setPermissions(dir.filePath("event0"), QFile::ReadOwner | QFile::WriteOwner));
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 12, 200);
        QTest::qWait(100); QCOMPARE(source.attempts.value("event0"), 12);
    }
    void retryReplacesConfigurationErrorWithPermissionStatus() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.setRetryPolicy(10, 20, 2); source.failConfiguration = true;
        source.readers.insert("event0", pipe.fds[0]); touch(dir.filePath("event0"));
        source.start({"key.w"}); QCOMPARE(source.status(), InputStatus::Error);
        source.failConfiguration = false; source.denied = true;
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 2, 200);
        QCOMPARE(source.status(), InputStatus::PermissionDenied);
        touch(dir.filePath("noise")); QTest::qWait(60);
        QCOMPARE(source.status(), InputStatus::PermissionDenied);
    }
    void timerRecoversWithoutHotplug() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.setRetryPolicy(10, 20, 4); source.denied = true;
        source.readers.insert("event0", pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source, &InputSource::inputChanged);
        source.start({"key.w"}); QCOMPARE(source.status(), InputStatus::PermissionDenied);
        source.denied = false;
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::Active, 200);
        QCOMPARE(source.attempts.value("event0"), 2);
        pipe.send(EV_KEY, KEY_W, 1);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 200);
        QCOMPARE(eventAt(spy, 0).id, "key.w");
        QTest::qWait(80); QCOMPARE(source.attempts.value("event0"), 2);
        QCOMPARE(source.findChildren<QTimer *>().size(), 1);
    }
    void pendingNodesShareOneRetryTimer() {
        QTemporaryDir dir; TestSource source(dir.path());
        source.setRetryPolicy(20, 40, 3); source.denied = true;
        touch(dir.filePath("event0")); touch(dir.filePath("event1"));
        source.start({"key.w"});
        QCOMPARE(source.findChildren<QTimer *>().size(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 3, 200);
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event1"), 3, 200);
        QCOMPARE(source.findChildren<QTimer *>().size(), 1);
        QVERIFY(QFile::setPermissions(dir.filePath("event0"), QFile::ReadOwner));
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 6, 200);
        QCOMPARE(source.attempts.value("event1"), 3);
        QTest::qWait(80); QCOMPARE(source.attempts.value("event1"), 3);
    }
    void disappearingFailedNodeCancelsRetry() {
        QTemporaryDir dir; TestSource source(dir.path());
        source.setRetryPolicy(100, 100, 3); source.denied = true;
        touch(dir.filePath("event0")); source.start({"key.w"});
        QCOMPARE(source.findChildren<QTimer *>().size(), 2);
        QVERIFY(QFile::remove(dir.filePath("event0")));
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::NoDevices, 200);
        QCOMPARE(source.findChildren<QTimer *>().size(), 1);
        QTest::qWait(120); QCOMPARE(source.attempts.value("event0"), 1);
    }
    void exactAttributeRecoversDormantNode() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.setRetryPolicy(5, 10, 1); source.denied = true;
        source.readers.insert("event0", pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source, &InputSource::inputChanged);
        source.start({"key.w"}); QCOMPARE(source.status(), InputStatus::PermissionDenied);
        source.denied = false;
        touch(dir.filePath("event1"));
        QVERIFY(QFile::setPermissions(dir.filePath("event1"), QFile::ReadOwner));
        QTest::qWait(60);
        QCOMPARE(source.attempts.value("event0"), 1);
        QCOMPARE(source.status(), InputStatus::PermissionDenied);
        QVERIFY(QFile::setPermissions(dir.filePath("event0"), QFile::ReadOwner));
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::Active, 200);
        QCOMPARE(source.attempts.value("event0"), 2);
        pipe.send(EV_KEY, KEY_A, 1); pipe.send(EV_KEY, KEY_W, 1);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 200);
        QCOMPARE(eventAt(spy, 0).id, "key.w"); QVERIFY(eventAt(spy, 0).down);
    }
    void deleteRecreateRecoversDormantNode() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.setRetryPolicy(5, 10, 1); source.denied = true;
        source.readers.insert("event0", pipe.fds[0]); touch(dir.filePath("event0"));
        source.start({"key.w"}); QCOMPARE(source.attempts.value("event0"), 1);
        source.denied = false;
        QVERIFY(QFile::remove(dir.filePath("event0"))); touch(dir.filePath("event0"));
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::Active, 200);
        QCOMPARE(source.attempts.value("event0"), 2);
    }
    void rapidEventsCoalesceScans() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0", pipe.fds[0]); source.rejected = true;
        touch(dir.filePath("event0")); countedPath = dir.filePath("event0");
        source.start({"key.w"}); QCOMPARE(identityChecks, 1);
        // Drain inotify repeatedly during the burst, so kernel coalescing alone cannot pass.
        for (int i = 0; i < 500; ++i) {
            touch(dir.filePath(QString("noise%1").arg(i)));
            QCoreApplication::processEvents();
        }
        QTest::qWait(70);
        QVERIFY(identityChecks > 1); QVERIFY(identityChecks < 25);
        qInfo("500 rapid events caused %d scans after startup", identityChecks - 1);
        QCOMPARE(source.attempts.value("event0"), 1);
    }
    void emptyCacheHasNoRetryTimer() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.setRetryPolicy(5, 10, 6); source.start({"key.w"});
        QTest::qWait(60); QVERIFY(source.attemptTimes.isEmpty());
        for (auto *timer : source.findChildren<QTimer *>()) QVERIFY(!timer->isActive());
        source.denied = true; touch(dir.filePath("event0"));
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::PermissionDenied, 200);
        source.denied = false; source.readers.insert("event0", pipe.fds[0]);
        QVERIFY(QFile::setPermissions(dir.filePath("event0"), QFile::ReadOwner));
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::Active, 200);
        const int attempts = source.attempts.value("event0");
        QTest::qWait(80); QCOMPARE(source.attempts.value("event0"), attempts);
        for (auto *timer : source.findChildren<QTimer *>()) QVERIFY(!timer->isActive());
        QVERIFY(QFile::remove(dir.filePath("event0")));
        QTRY_COMPARE_WITH_TIMEOUT(source.status(), InputStatus::NoDevices, 200);
        QTest::qWait(60); QCOMPARE(source.attempts.value("event0"), attempts);
    }
    void stopAndRestartResetFailuresAndTimers() {
        QTemporaryDir dir; TestSource source(dir.path());
        source.setRetryPolicy(10, 20, 3); source.denied = true;
        touch(dir.filePath("event0")); source.start({"key.w"});
        touch(dir.filePath("noise")); QCoreApplication::processEvents();
        source.stop();
        for (auto *timer : source.findChildren<QTimer *>()) QVERIFY(!timer->isActive());
        QTest::qWait(80); QCOMPARE(source.attempts.value("event0"), 1);
        source.start({"key.w"}); QCOMPARE(source.attempts.value("event0"), 2);
        QTRY_COMPARE_WITH_TIMEOUT(source.attempts.value("event0"), 4, 200);
        source.stop(); source.start({"key.w"});
        QCOMPARE(source.attempts.value("event0"), 5);
    }
    void destructionWithPendingTimers() {
        QTemporaryDir dir;
        auto source = std::make_unique<TestSource>(dir.path());
        source->setRetryPolicy(100, 100, 3); source->denied = true;
        touch(dir.filePath("event0")); source->start({"key.w"});
        touch(dir.filePath("noise")); QCoreApplication::processEvents();
        QList<QPointer<QTimer>> timers;
        for (auto *timer : source->findChildren<QTimer *>()) {
            QVERIFY(timer->isActive()); timers.append(timer);
        }
        QCOMPARE(timers.size(), 2);
        source.reset();
        for (const auto &timer : timers) QVERIFY(timer.isNull());
        QTest::qWait(120);
    }
    void allowlistAndLifecycle() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0", pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source, &InputSource::inputChanged);
        source.start({"key.w", "mouse.left", "mouse.wheelup"});
        QCOMPARE(source.status(), InputStatus::Active);
        source.start({"key.a"}); // Idempotent: does not replace the active allowlist.
        const auto before = monotonicMs();
        pipe.send(EV_KEY, KEY_A, 1); pipe.send(EV_MSC, MSC_SCAN, 17);
        pipe.send(EV_KEY, KEY_W, 1); pipe.send(EV_KEY, KEY_W, 2);
        pipe.send(EV_KEY, BTN_LEFT, 1); pipe.send(EV_REL, REL_WHEEL, 1);
        QTRY_COMPARE(spy.size(), 4);
        QCOMPARE(eventAt(spy, 0).id, "key.w"); QVERIFY(eventAt(spy, 0).down);
        QVERIFY(eventAt(spy, 0).tMs >= before); QVERIFY(eventAt(spy, 0).tMs <= monotonicMs());
        QCOMPARE(eventAt(spy, 1).id, "mouse.left");
        QCOMPARE(eventAt(spy, 2).id, "mouse.wheelup"); QVERIFY(eventAt(spy, 2).down);
        QVERIFY(!eventAt(spy, 3).down);
        source.stop(); QCOMPARE(spy.size(), 6); QCOMPARE(source.status(), InputStatus::Stopped);
        QVERIFY(!eventAt(spy, 4).down); QVERIFY(!eventAt(spy, 5).down);
        source.stop(); QCOMPARE(spy.size(), 6);
        for (int fd : source.opened) { errno = 0; QCOMPARE(fcntl(fd,F_GETFD),-1); QCOMPARE(errno,EBADF); }
        for (const auto &args : spy) QVERIFY(QSet<InputId>({"key.w", "mouse.left", "mouse.wheelup"}).contains(qvariant_cast<InputEvent>(args.at(0)).id));
        source.start({"key.a"}); pipe.send(EV_KEY, KEY_A, 1); QTRY_COMPARE(spy.size(), 7);
        QCOMPARE(eventAt(spy, 6).id, "key.a");
    }
    void permissionsAndHotplug() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        QSignalSpy statuses(&source, &InputSource::statusChanged);
        QSignalSpy spy(&source, &InputSource::inputChanged);
        source.start({"key.w"}); QCOMPARE(source.status(), InputStatus::NoDevices);
        source.denied = true; touch(dir.filePath("event0"));
        QTRY_COMPARE(source.status(), InputStatus::PermissionDenied);
        QVERIFY(!source.statusDetail().isEmpty()); QVERIFY(!source.statusDetail().contains("event0"));
        source.denied = false; source.readers.insert("event0", pipe.fds[0]);
        QVERIFY(QFile::setPermissions(dir.filePath("event0"), QFile::ReadOwner | QFile::WriteOwner));
        QTRY_COMPARE(source.status(), InputStatus::Active);
        pipe.send(EV_KEY, KEY_W, 1); QTRY_COMPARE(spy.size(), 1);
        QVERIFY(QFile::remove(dir.filePath("event0")));
        QTRY_COMPARE(source.status(), InputStatus::NoDevices); QCOMPARE(spy.size(), 2);
        QVERIFY(!eventAt(spy, 1).down);
        source.stop(); touch(dir.filePath("event0")); QTest::qWait(20);
        QCOMPARE(source.status(), InputStatus::Stopped);
        QCOMPARE(statuses.size(), 5);
    }
    void dualSense() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0", pipe.fds[0]); touch(dir.filePath("event0"));
        const QList<QPair<int, QString>> buttons{{BTN_SOUTH,"pad.south"},{BTN_EAST,"pad.east"},
            {BTN_WEST,"pad.west"},{BTN_NORTH,"pad.north"},{BTN_TL,"pad.l1"},{BTN_TR,"pad.r1"},
            {BTN_TL2,"pad.l2"},{BTN_TR2,"pad.r2"},{BTN_SELECT,"pad.select"},
            {BTN_START,"pad.start"},{BTN_MODE,"pad.mode"},{BTN_THUMBL,"pad.l3"},{BTN_THUMBR,"pad.r3"}};
        QSet<InputId> allow{"pad.dpad.up","pad.dpad.down","pad.dpad.left","pad.dpad.right"};
        for (const auto &b : buttons) allow.insert(b.second);
        QSignalSpy spy(&source, &InputSource::inputChanged); source.start(allow);
        for (const auto &b : buttons) { pipe.send(EV_KEY, b.first, 1); pipe.send(EV_KEY, b.first, 0); }
        QTRY_COMPARE(spy.size(), buttons.size() * 2);
        for (int i = 0; i < buttons.size(); ++i) {
            QCOMPARE(eventAt(spy, i*2).id, buttons[i].second); QVERIFY(eventAt(spy, i*2).down);
            QCOMPARE(eventAt(spy, i*2+1).id, buttons[i].second); QVERIFY(!eventAt(spy, i*2+1).down);
        }
        spy.clear();
        pipe.send(EV_ABS, ABS_HAT0X, -1); pipe.send(EV_ABS, ABS_HAT0X, 1); pipe.send(EV_ABS, ABS_HAT0X, 0);
        pipe.send(EV_ABS, ABS_HAT0Y, -1); pipe.send(EV_ABS, ABS_HAT0Y, 1); pipe.send(EV_ABS, ABS_HAT0Y, 0);
        QTRY_COMPARE(spy.size(), 8);
        const QStringList expected{"pad.dpad.left","pad.dpad.left","pad.dpad.right","pad.dpad.right",
            "pad.dpad.up","pad.dpad.up","pad.dpad.down","pad.dpad.down"};
        for (int i=0; i<8; ++i) { QCOMPARE(eventAt(spy,i).id,expected[i]); QCOMPARE(eventAt(spy,i).down, i%2 == 0); }
        spy.clear();
        pipe.send(EV_ABS, ABS_Z, 127); pipe.send(EV_ABS, ABS_Z, 128);
        pipe.send(EV_KEY, BTN_TL2, 1); pipe.send(EV_ABS, ABS_Z, 0); pipe.send(EV_KEY, BTN_TL2, 0);
        pipe.send(EV_ABS, ABS_RZ, 255); pipe.send(EV_ABS, ABS_RZ, 0);
        pipe.send(EV_ABS, ABS_X, 255);
        QTRY_COMPARE(spy.size(), 4);
        QCOMPARE(eventAt(spy,0).id,"pad.l2"); QVERIFY(eventAt(spy,0).down);
        QCOMPARE(eventAt(spy,1).id,"pad.l2"); QVERIFY(!eventAt(spy,1).down);
        QCOMPARE(eventAt(spy,2).id,"pad.r2"); QVERIFY(eventAt(spy,2).down);
        QCOMPARE(eventAt(spy,3).id,"pad.r2"); QVERIFY(!eventAt(spy,3).down);
    }
    void partialReadsAndDisconnect() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0",pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"key.w"});
        input_event e{}; e.type=EV_KEY; e.code=KEY_W; e.value=1;
        QCOMPARE(write(pipe.fds[1],&e,7),7); QTest::qWait(20); QCOMPARE(spy.size(),0);
        QCOMPARE(write(pipe.fds[1],reinterpret_cast<char*>(&e)+7,sizeof(e)-7),ssize_t(sizeof(e)-7));
        QTRY_COMPARE(spy.size(),1);
        close(pipe.fds[1]); pipe.fds[1]=-1;
        QTRY_COMPARE(spy.size(),2); QVERIFY(!eventAt(spy,1).down);
        QCOMPARE(source.status(),InputStatus::NoDevices);
    }
    void multipleDevicesAndDroppedEvents() {
        QTemporaryDir dir; Pipe a,b; TestSource source(dir.path());
        source.readers.insert("event0",a.fds[0]); source.readers.insert("event1",b.fds[0]);
        touch(dir.filePath("event0")); touch(dir.filePath("event1"));
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"key.w"});
        a.send(EV_KEY,KEY_W,1); b.send(EV_KEY,KEY_W,1); QTRY_COMPARE(spy.size(),1);
        QTest::qWait(20); QVERIFY(QFile::remove(dir.filePath("event0"))); QTest::qWait(20); QCOMPARE(spy.size(),1);
        b.send(EV_SYN,SYN_DROPPED,0); b.send(EV_KEY,KEY_W,1); b.send(EV_SYN,SYN_REPORT,0);
        QTRY_COMPARE(spy.size(),2); QVERIFY(!eventAt(spy,1).down);
        b.send(EV_KEY,KEY_W,1); QTRY_COMPARE(spy.size(),3);
    }
    void realProbeRejectsNonDevices() {
        QTemporaryDir dir; touch(dir.filePath("event0"));
        EvdevInputSource source(dir.path()); source.start({"key.w"});
        QCOMPARE(source.status(),InputStatus::NoDevices);
    }
    void emptyIdentifierIsNeverEmitted() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0",pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"", "key.w"});
        pipe.send(EV_KEY, KEY_BRIGHTNESSUP, 1); pipe.send(EV_KEY, KEY_W, 1);
        QTRY_VERIFY(!spy.isEmpty()); QTest::qWait(20);
        QCOMPARE(spy.size(),1); QCOMPARE(eventAt(spy,0).id,"key.w");
    }
    void missingDirectoryAndReplacement() {
        QTemporaryDir root; const auto directory = root.filePath("input");
        Pipe oldPipe, newPipe; TestSource source(directory);
        QSignalSpy spy(&source,&InputSource::inputChanged);
        source.start({"key.w"}); QCOMPARE(source.status(),InputStatus::NoDevices);
        source.readers.insert("event0",oldPipe.fds[0]);
        QVERIFY(QDir().mkdir(directory)); touch(directory + "/event0");
        QTRY_COMPARE(source.status(),InputStatus::Active);
        oldPipe.send(EV_KEY,KEY_W,1); QTRY_COMPARE(spy.size(),1);
        QVERIFY(QFile::remove(directory + "/event0"));
        source.readers.insert("event0",newPipe.fds[0]); touch(directory + "/event0");
        QTRY_COMPARE(spy.size(),2); QVERIFY(!eventAt(spy,1).down);
        newPipe.send(EV_KEY,KEY_W,1); QTRY_COMPARE(spy.size(),3);
        QVERIFY(eventAt(spy,2).down);
    }
    void stopDuringRemoval() {
        QTemporaryDir dir; Pipe a,b; TestSource source(dir.path());
        source.readers.insert("event0",a.fds[0]); source.readers.insert("event1",b.fds[0]);
        touch(dir.filePath("event0"));
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"key.w","key.a"});
        a.send(EV_KEY,KEY_W,1); a.send(EV_KEY,KEY_A,1); QTRY_COMPARE(spy.size(),2);
        connect(&source,&InputSource::inputChanged,&source,[&](const InputEvent &event) {
            if (!event.down) source.stop();
        });
        QVERIFY(QFile::remove(dir.filePath("event0"))); touch(dir.filePath("event1"));
        QTRY_COMPARE(source.status(),InputStatus::Stopped);
        b.send(EV_KEY,KEY_W,1); QTest::qWait(20); QCOMPARE(spy.size(),4);
        QSet<InputId> released{eventAt(spy,2).id,eventAt(spy,3).id};
        QCOMPARE(released,QSet<InputId>({"key.w","key.a"}));
        QVERIFY(!eventAt(spy,2).down); QVERIFY(!eventAt(spy,3).down);
    }
    void restoresHeldKeysAfterOverflow() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0",pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"key.w"});
        snapshotFd = source.opened.first();
        pipe.send(EV_KEY,KEY_W,1); pipe.send(EV_SYN,SYN_DROPPED,0);
        pipe.send(EV_SYN,SYN_REPORT,0);
        QTRY_COMPARE(spy.size(),3);
        QCOMPARE(eventAt(spy,2).id,"key.w"); QVERIFY(eventAt(spy,2).down);
    }
    void reportsInputsAlreadyHeldWhenTheDeviceIsOpened() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.readers.insert("event0",pipe.fds[0]); touch(dir.filePath("event0"));
        source.holdWOnOpen = true;                      // W is physically held before the source starts
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"key.w","key.a"});
        QCOMPARE(spy.size(),1);                          // the held key is reported, nothing outside the allowlist
        QCOMPARE(eventAt(spy,0).id,"key.w"); QVERIFY(eventAt(spy,0).down);
        pipe.send(EV_KEY,KEY_W,0); pipe.send(EV_SYN,SYN_REPORT,0);
        QTRY_COMPARE(spy.size(),2); QVERIFY(!eventAt(spy,1).down);
    }
    void triggerCalibrationAndAllowlist() {
        QTemporaryDir dir; Pipe pipe; TestSource source(dir.path());
        source.triggerRange(-100,900);
        source.readers.insert("event0",pipe.fds[0]); touch(dir.filePath("event0"));
        QSignalSpy spy(&source,&InputSource::inputChanged); source.start({"pad.l2"});
        pipe.send(EV_ABS,ABS_Z,400); pipe.send(EV_ABS,ABS_RZ,900);
        pipe.send(EV_ABS,ABS_HAT0Y,-1); pipe.send(EV_ABS,ABS_Z,401);
        pipe.send(EV_ABS,ABS_Z,-100);
        QTRY_COMPARE(spy.size(),2);
        QCOMPARE(eventAt(spy,0).id,"pad.l2"); QVERIFY(eventAt(spy,0).down);
        QCOMPARE(eventAt(spy,1).id,"pad.l2"); QVERIFY(!eventAt(spy,1).down);
    }
    void permissionAndUnexpectedOpenErrors() {
        QTemporaryDir dir; touch(dir.filePath("event0"));
        TestSource source(dir.path()); source.openError = EPERM;
        QSignalSpy statuses(&source,&InputSource::statusChanged);
        source.start({"key.w"}); QCOMPARE(source.status(),InputStatus::PermissionDenied);
        QCOMPARE(qvariant_cast<InputStatus>(statuses.at(0).at(0)),InputStatus::PermissionDenied);
        source.stop(); source.openError = EMFILE; source.start({"key.w"});
        QCOMPARE(source.status(),InputStatus::Error); QVERIFY(!source.statusDetail().isEmpty());
        QVERIFY(QFile::remove(dir.filePath("event0")));
        QTRY_COMPARE(source.status(),InputStatus::NoDevices);
    }
    void keymap_data() {
        QTest::addColumn<int>("code"); QTest::addColumn<QString>("id");
        QTest::newRow("physical-a") << int(KEY_A) << QString("key.a");
        QTest::newRow("digit") << int(KEY_0) << QString("key.0");
        QTest::newRow("function") << int(KEY_F12) << QString("key.f12");
        QTest::newRow("modifier") << int(KEY_RIGHTCTRL) << QString("key.rctrl");
        QTest::newRow("punctuation") << int(KEY_APOSTROPHE) << QString("key.apostrophe");
        QTest::newRow("arrow") << int(KEY_LEFT) << QString("key.left");
        QTest::newRow("back") << int(BTN_SIDE) << QString("mouse.back");
        QTest::newRow("forward") << int(BTN_EXTRA) << QString("mouse.forward");
        QTest::newRow("unknown") << int(KEY_BRIGHTNESSUP) << QString();
    }
    void keymap() { QFETCH(int,code); QFETCH(QString,id); QCOMPARE(Evdev::keyId(code),id); }
};
QTEST_GUILESS_MAIN(EvdevTests)
#include "tst_evdev_source.moc"
