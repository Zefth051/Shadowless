#include "evdev_source.h"
#include "evdev_keymap.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSocketNotifier>
#include <array>
#include <map>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace Shadowless::Input {
namespace {
template<size_t N> bool bit(const std::array<unsigned long, N> &bits, unsigned int n) {
    constexpr auto width = sizeof(unsigned long) * 8;
    return n / width < N && (bits[n / width] & (1UL << (n % width)));
}
constexpr size_t keyWords = (KEY_MAX / (sizeof(unsigned long) * 8)) + 1;
}

struct EvdevInputSource::Impl {
    struct Device {
        int fd = -1;
        QSocketNotifier *notifier = nullptr;
        QByteArray pending;
        DeviceInfo info;
        QSet<InputId> digital, analog, held;
        bool dropped = false;
        ~Device() { delete notifier; if (fd >= 0) close(fd); }
    };
    EvdevInputSource *q;
    QString directory;
    bool started = false, denied = false;
    int monitor = -1, directoryWatch = -1, parentWatch = -1;
    QSocketNotifier *monitorNotifier = nullptr;
    QSet<InputId> allow;
    QHash<InputId, int> heldCounts;
    std::map<QString, std::shared_ptr<Device>> devices;
    QString failure, monitorFailure;

    Impl(EvdevInputSource *source, QString path) : q(source), directory(QDir::cleanPath(path)) {}
    void status() {
        if (!started) return;
        if (!devices.empty()) q->setStatus(InputStatus::Active);
        else if (denied) q->setStatus(InputStatus::PermissionDenied,
            QStringLiteral("Input device access denied. See the input permission settings for a narrow seat-session udev rule."));
        else if (!failure.isEmpty()) q->setStatus(InputStatus::Error, failure);
        else q->setStatus(InputStatus::NoDevices);
    }
    void edge(const std::shared_ptr<Device> &device, const InputId &id, bool down, bool analog = false) {
        if (id.isEmpty() || !allow.contains(id)) return;
        auto &states = analog ? device->analog : device->digital;
        if (down) states.insert(id); else states.remove(id);
        const bool held = device->digital.contains(id) || device->analog.contains(id);
        if (device->held.contains(id) == held) return;
        if (held) device->held.insert(id); else device->held.remove(id);
        const int previous = heldCounts.value(id);
        const int current = previous + (held ? 1 : -1);
        if (current) heldCounts.insert(id, current); else heldCounts.remove(id);
        if ((previous == 0) != (current == 0))
            emit q->inputChanged(InputEvent{id, current != 0, monotonicMs()});
    }
    void release(const std::shared_ptr<Device> &device) {
        device->digital.clear(); device->analog.clear();
        const auto held = device->held;
        for (const auto &id : held) edge(device, id, false);
    }
    void remove(const QString &path) {
        const auto it = devices.find(path);
        if (it == devices.end()) return;
        auto device = it->second;
        device->notifier->setEnabled(false);
        // Keep it visible to stop() until release callbacks have finished.
        release(device);
        const auto current = devices.find(path);
        if (current != devices.end() && current->second == device) devices.erase(current);
    }
    void resync(const std::shared_ptr<Device> &device) {
        std::array<unsigned long, keyWords> keys{};
        if (ioctl(device->fd, EVIOCGKEY(sizeof(keys)), keys.data()) >= 0) {
            for (unsigned int code = 0; code <= KEY_MAX && started; ++code)
                if (bit(keys, code)) edge(device, Evdev::keyId(code), true);
        }
        for (unsigned short code : {ABS_HAT0X, ABS_HAT0Y, ABS_Z, ABS_RZ}) {
            if (!started) return;
            input_absinfo axis{};
            if (ioctl(device->fd, EVIOCGABS(code), &axis) == 0) {
                input_event event{}; event.type = EV_ABS; event.code = code; event.value = axis.value;
                process(device, event);
            }
        }
    }
    void process(const std::shared_ptr<Device> &device, const input_event &event) {
        if (event.type == EV_SYN && event.code == SYN_DROPPED) {
            device->dropped = true;
            release(device);
            return;
        }
        if (device->dropped) {
            if (event.type == EV_SYN && event.code == SYN_REPORT) {
                device->dropped = false;
                resync(device);
            }
            return;
        }
        if (event.type == EV_KEY && (event.value == 0 || event.value == 1)) {
            edge(device, Evdev::keyId(event.code), event.value == 1);
        } else if (event.type == EV_REL && event.code == REL_WHEEL && event.value != 0) {
            const auto id = event.value > 0 ? QStringLiteral("mouse.wheelup") : QStringLiteral("mouse.wheeldown");
            edge(device, id, true); edge(device, id, false);
        } else if (event.type == EV_ABS) {
            if (event.code == ABS_HAT0X) {
                edge(device, QStringLiteral("pad.dpad.left"), event.value < 0, true);
                edge(device, QStringLiteral("pad.dpad.right"), event.value > 0, true);
            } else if (event.code == ABS_HAT0Y) {
                edge(device, QStringLiteral("pad.dpad.up"), event.value < 0, true);
                edge(device, QStringLiteral("pad.dpad.down"), event.value > 0, true);
            } else if (event.code == ABS_Z || event.code == ABS_RZ) {
                const bool left = event.code == ABS_Z;
                const qint64 minimum = left ? device->info.leftMinimum : device->info.rightMinimum;
                const qint64 maximum = left ? device->info.leftMaximum : device->info.rightMaximum;
                edge(device, left ? QStringLiteral("pad.l2") : QStringLiteral("pad.r2"),
                     maximum > minimum && (qint64(event.value) - minimum) * 2 > maximum - minimum, true);
            }
        }
    }
    void readDevice(const QString &path, const std::shared_ptr<Device> &device) {
        // Bound each activation so an input flood cannot starve the owning Qt thread.
        for (int batch = 0; batch < 16 && started && devices.count(path); ++batch) {
            char buffer[sizeof(input_event) * 64];
            const auto count = read(device->fd, buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            if (count <= 0) {
                if (count < 0 && (errno == EACCES || errno == EPERM)) denied = true;
                else if (count < 0 && errno != ENODEV && errno != EIO)
                    failure = QStringLiteral("Unable to read an input device.");
                remove(path); status(); return;
            }
            device->pending.append(buffer, count);
            while (device->pending.size() >= qsizetype(sizeof(input_event))) {
                input_event event;
                std::memcpy(&event, device->pending.constData(), sizeof(event));
                device->pending.remove(0, sizeof(event));
                process(device, event);
                if (!started || !devices.count(path)) return;
            }
        }
    }
    void watchDirectory() {
        if (monitor < 0) return;
        const uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB
                            | IN_DELETE_SELF | IN_MOVE_SELF;
        if (directoryWatch < 0) {
            directoryWatch = inotify_add_watch(monitor, QFile::encodeName(directory).constData(), mask);
            if (directoryWatch < 0) {
                if (errno == EACCES || errno == EPERM) denied = true;
                else if (errno != ENOENT) failure = QStringLiteral("Unable to watch the input directory.");
            }
        }
        if (parentWatch < 0) parentWatch = inotify_add_watch(monitor,
            QFile::encodeName(QFileInfo(directory).absolutePath()).constData(), mask);
    }
    void scan() {
        if (!started) return;
        failure = monitorFailure;
        denied = false;
        watchDirectory();
        const QDir dir(directory);
        const auto entries = dir.entryList({QStringLiteral("event*")}, QDir::System | QDir::Files | QDir::NoDotAndDotDot);
        QSet<QString> paths;
        for (const auto &entry : entries) {
            bool numeric = entry.size() > 5;
            for (auto ch : entry.mid(5)) numeric = numeric && ch >= QLatin1Char('0') && ch <= QLatin1Char('9');
            if (numeric) paths.insert(dir.filePath(entry));
        }
        QStringList missing;
        for (const auto &entry : devices) if (!paths.contains(entry.first)) missing.append(entry.first);
        for (const auto &path : missing) {
            remove(path);
            if (!started) return;
        }
        for (const auto &path : paths) {
            if (!started) return;
            if (devices.count(path)) continue;
            const int fd = q->openDevice(path);
            if (fd < 0) {
                if (errno == EACCES || errno == EPERM) denied = true;
                else if (errno != ENOENT && errno != ENODEV)
                    failure = QStringLiteral("Unable to open an input device.");
                continue;
            }
            auto device = std::make_shared<Device>(); device->fd = fd;
            const int flags = fcntl(fd, F_GETFL);
            if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 || fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                failure = QStringLiteral("Unable to configure an input device."); continue;
            }
            if (!q->probeDevice(fd, &device->info)) continue;
            device->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, q);
            devices.emplace(path, device);
            // The map owns devices; weak capture avoids a notifier/device ownership cycle.
            std::weak_ptr<Device> weak = device;
            QObject::connect(device->notifier, &QSocketNotifier::activated, q, [this, path, weak] {
                if (auto current = weak.lock()) readDevice(path, current);
            });
        }
        status();
    }
    void hotplug() {
        alignas(inotify_event) char buffer[4096];
        for (;;) {
            const auto count = read(monitor, buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (count <= 0) {
                monitorFailure = QStringLiteral("Unable to monitor input devices.");
                monitorNotifier->setEnabled(false);
                break;
            }
            for (size_t offset = 0; offset + sizeof(inotify_event) <= size_t(count);) {
                const auto *event = reinterpret_cast<const inotify_event *>(buffer + offset);
                if (event->mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF)) {
                    if (event->wd == directoryWatch) directoryWatch = -1;
                    if (event->wd == parentWatch) parentWatch = -1;
                }
                // A remove/recreate with the same name must close the old descriptor too.
                if (event->wd == directoryWatch && event->len && (event->mask & (IN_DELETE | IN_MOVED_FROM)))
                    remove(QDir(directory).filePath(QString::fromLocal8Bit(event->name)));
                if (!started) return;
                offset += sizeof(inotify_event) + event->len;
            }
        }
        scan();
    }
};

EvdevInputSource::EvdevInputSource(QObject *parent) : EvdevInputSource(QStringLiteral("/dev/input"), parent) {}
EvdevInputSource::EvdevInputSource(const QString &directory, QObject *parent)
    : InputSource(parent), d(std::make_unique<Impl>(this, directory)) {}
EvdevInputSource::~EvdevInputSource() { stop(); }
void EvdevInputSource::start(const QSet<InputId> &allowlist) {
    if (d->started) return;
    d->started = true; d->allow = allowlist; d->failure.clear(); d->monitorFailure.clear();
    d->monitor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (d->monitor < 0) d->monitorFailure = QStringLiteral("Unable to monitor input devices.");
    else {
        d->monitorNotifier = new QSocketNotifier(d->monitor, QSocketNotifier::Read, this);
        connect(d->monitorNotifier, &QSocketNotifier::activated, this, [this] { d->hotplug(); });
    }
    d->scan();
}
void EvdevInputSource::stop() {
    if (!d->started) return;
    d->started = false;
    delete d->monitorNotifier; d->monitorNotifier = nullptr;
    if (d->monitor >= 0) close(d->monitor);
    d->monitor = d->directoryWatch = d->parentWatch = -1;
    auto devices = std::move(d->devices);
    d->devices.clear();
    for (const auto &entry : devices) { entry.second->notifier->setEnabled(false); d->release(entry.second); }
    d->allow.clear(); d->heldCounts.clear(); d->denied = false;
    setStatus(InputStatus::Stopped);
}
int EvdevInputSource::openDevice(const QString &path) {
    return open(QFile::encodeName(path).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
}
bool EvdevInputSource::probeDevice(int fd, DeviceInfo *info) {
    std::array<unsigned long, keyWords> keys{};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data()) < 0) return false;
    const bool keyboard = bit(keys, KEY_A) && bit(keys, KEY_Z) && bit(keys, KEY_ENTER);
    const bool mouse = bit(keys, BTN_LEFT);
    const bool gamepad = bit(keys, BTN_SOUTH) || bit(keys, BTN_EAST);
    if (!keyboard && !mouse && !gamepad) return false;
    input_absinfo axis{};
    if (gamepad && ioctl(fd, EVIOCGABS(ABS_Z), &axis) == 0) {
        info->leftMinimum = axis.minimum; info->leftMaximum = axis.maximum;
    } else info->leftMaximum = info->leftMinimum;
    if (gamepad && ioctl(fd, EVIOCGABS(ABS_RZ), &axis) == 0) {
        info->rightMinimum = axis.minimum; info->rightMaximum = axis.maximum;
    } else info->rightMaximum = info->rightMinimum;
    return true;
}
}
