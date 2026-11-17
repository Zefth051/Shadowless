#include "input_access.h"
#include <QCoreApplication>
#include <QDebug>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

namespace Shadowless::Input {
namespace {
constexpr const char *kRuleFile = "/etc/udev/rules.d/71-shadowless-input-access.rules";
constexpr int kKeyA = 30, kKeyZ = 44, kKeyEnter = 28, kBtnLeft = 272, kBtnSouth = 304, kBtnEast = 305;

// True when root controls the file and every directory above it, so nobody else can swap it between authorization and execution.
bool rootControlledImpl(const QString &path) {
    QString current = QFileInfo(path).absoluteFilePath();
    while (true) {
        struct stat st {};
        if (::lstat(QFile::encodeName(current).constData(), &st) != 0 || st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH)) || S_ISLNK(st.st_mode))
            return false;
        if (current == "/") return true;
        current = QFileInfo(current).absolutePath();
    }
}

QString attribute(const QString &path) {
    QFile file(path);
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.read(4096)).trimmed();
}
// sysfs prints capability words as hex, highest word first, one machine word each.
bool hasKey(const QStringList &words, int code) {
    const int bits = int(sizeof(unsigned long) * 8), index = code / bits;
    if (index >= words.size()) return false;
    bool ok = false;
    const quint64 word = words.at(words.size() - 1 - index).toULongLong(&ok, 16);
    return ok && (word >> (code % bits)) & 1;
}
QString className(AccessDevice::Class cls) {
    return cls == AccessDevice::Keyboard ? "keyboard" : cls == AccessDevice::Mouse ? "mouse" : "gamepad";
}
} // namespace

QString AccessDevice::triple() const { return vendor + ":" + product + ":" + className(cls); }

bool InputAccess::validTriple(const QString &triple) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{4}:[0-9a-f]{4}:(keyboard|mouse|gamepad)\\z"));
    return pattern.match(triple).hasMatch();
}

QVector<AccessDevice> InputAccess::enumerate(const QString &sysRoot, const QString &devRoot) {
    QVector<AccessDevice> devices;
    static const QRegularExpression eventPattern(QStringLiteral("^event([0-9]+)$"));
    const auto names = QDir(sysRoot).entryList({"event*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &entry : names) {
        const auto match = eventPattern.match(entry);
        if (!match.hasMatch()) continue;
        const QString base = sysRoot + "/" + entry + "/device";
        AccessDevice device;
        device.name = attribute(base + "/name").remove(QRegularExpression(QStringLiteral("[^\\P{C}]"))).left(80);
        device.vendor = attribute(base + "/id/vendor").toLower();
        device.product = attribute(base + "/id/product").toLower();
        device.node = devRoot + "/" + entry;
        device.eventNumber = match.captured(1).toInt();
        const QStringList keys = attribute(base + "/capabilities/key").split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        bool evOk = false;
        const quint64 evBits = attribute(base + "/capabilities/ev").toULongLong(&evOk, 16);
        bool relOk = false;
        const quint64 relBits = attribute(base + "/capabilities/rel").toULongLong(&relOk, 16);
        const bool repeats = evOk && ((evBits >> 20) & 1);                   // EV_REP: only real keyboards autorepeat
        const bool pointer = relOk && (relBits & 3) == 3;                    // REL_X and REL_Y: excludes touchpads and tablets
        if (hasKey(keys, kBtnSouth) || hasKey(keys, kBtnEast)) device.cls = AccessDevice::Gamepad;
        else if (repeats && hasKey(keys, kKeyA) && hasKey(keys, kKeyZ) && hasKey(keys, kKeyEnter)) device.cls = AccessDevice::Keyboard;
        else if (pointer && hasKey(keys, kBtnLeft)) device.cls = AccessDevice::Mouse;
        else continue;
        if (!validTriple(device.triple()) || (device.vendor == "0000" && device.product == "0000")) continue;   // virtual or anonymous devices: a rule would match every one of them
        device.readable = ::access(QFile::encodeName(device.node).constData(), R_OK) == 0;
        devices.append(device);
    }
    return devices;
}

AccessDevice AccessDevice::asClass(Class other) const {
    AccessDevice result = *this;
    result.cls = other;
    result.readable = true;
    bool any = false;
    for (const auto &member : members) if (member.cls == other) { any = true; result.readable = result.readable && member.readable; }
    if (!any) result.readable = false;
    result.alternatives.clear();
    for (const auto &member : members) if (member.cls != other && !result.alternatives.contains(member.cls)) result.alternatives.append(member.cls);
    return result;
}

QVector<AccessDevice> InputAccess::groupDevices(const QVector<AccessDevice> &nodes) {
    QVector<AccessDevice> groups;
    QStringList order;
    QHash<QString, QVector<AccessDevice>> byIdentity;
    for (const auto &node : nodes) {
        if (!byIdentity.contains(node.identity())) order.append(node.identity());
        byIdentity[node.identity()].append(node);
    }
    for (const QString &identity : order) {
        const auto &members = byIdentity[identity];
        int counts[3] = {0, 0, 0};
        QString base = members.first().name;
        for (const auto &m : members) { ++counts[m.cls]; if (m.name.size() < base.size()) base = m.name; }
        using C = AccessDevice::Class;
        C chosen = C::Keyboard;
        bool confident = true;
        const bool mouseName = base.contains(QRegularExpression("\\b(mouse|mice|trackball)\\b", QRegularExpression::CaseInsensitiveOption));
        const bool keyboardName = base.contains(QRegularExpression("\\b(keyboard|keypad)\\b", QRegularExpression::CaseInsensitiveOption));
        if (counts[C::Gamepad] > 0) chosen = C::Gamepad;
        else if (counts[C::Mouse] > 0 && counts[C::Keyboard] == 0) chosen = C::Mouse;
        else if (counts[C::Mouse] == 0) chosen = C::Keyboard;
        else if (mouseName && !keyboardName) chosen = C::Mouse;
        else if (keyboardName && !mouseName) chosen = C::Keyboard;
        else if (counts[C::Mouse] != counts[C::Keyboard]) chosen = counts[C::Mouse] > counts[C::Keyboard] ? C::Mouse : C::Keyboard;  // the larger interface set wins
        else confident = false;                                                           // true hybrid: keep the keyboard default and offer the alternative
        AccessDevice group = members.first();
        group.name = base;
        group.members = members;
        for (const auto &m : members) group.eventNumber = qMin(group.eventNumber, m.eventNumber);
        group = group.asClass(chosen);
        if (confident) group.alternatives.clear();
        groups.append(group);
    }
    return groups;
}

QStringList InputAccess::triples(const QVector<AccessDevice> &devices) {
    QSet<QString> unique;
    for (const auto &device : devices) if (validTriple(device.triple())) unique.insert(device.triple());
    QStringList result(unique.cbegin(), unique.cend());
    result.sort();
    return result;
}

QVector<AccessDevice> InputAccess::primaryNodes(const QVector<AccessDevice> &devices) {
    QVector<AccessDevice> result;
    QSet<QString> seen;
    QVector<AccessDevice> sorted = devices;
    std::sort(sorted.begin(), sorted.end(), [](const AccessDevice &a, const AccessDevice &b) { return a.eventNumber < b.eventNumber; });
    for (const auto &device : sorted) if (!seen.contains(device.identity())) { seen.insert(device.identity()); result.append(device); }
    return result;
}

bool InputAccess::allReadable(const QVector<AccessDevice> &devices) {
    return std::all_of(devices.cbegin(), devices.cend(), [](const AccessDevice &d) { return d.readable; });
}

InputAccess::Result InputAccess::resultForExit(int exitCode, QProcess::ExitStatus status, const QString &output) {
    if (status != QProcess::NormalExit) return Result::Failed;
    if (exitCode == 0) return Result::Granted;
    const auto has = [&output](const char *text) { return output.contains(QLatin1String(text), Qt::CaseInsensitive); };
    if (exitCode == 126 || exitCode == 127) {
        // pkexec: 126 = the dialog was dismissed. 127 = everything else (not authorized, no agent, polkit missing, exec failure).
        // sh also returns 126 when it cannot execute the staged helper, so the tool output decides.
        if (has("authentication agent")) return Result::NoAgent;
        if (has("not authorized") || has("authentication failed") || has("authentication error")) return Result::NotAuthorized;
        if (has("error getting authority") || has("must be setuid root") || has("polkit") || has("pkexec: command not found")) return Result::PolkitUnavailable;
        if (has("permission denied") || has("not executable") || has("noexec") || has("operation not permitted") || has("no such file") || has("not found") || has("sha256"))
            return Result::HelperBlocked;
        if (exitCode == 126) return Result::Cancelled;   // dismissed (pkexec prints "Request dismissed" or nothing)
        return Result::Failed;
    }
    if (exitCode == 4) return Result::AppliedNeedsReload;
    return Result::Failed;
}

QString InputAccess::messageFor(Result result, int exitCode, const QString &output) {
    switch (result) {
    case Result::Granted: return QStringLiteral("Input access enabled.");
    case Result::Cancelled: return QStringLiteral("Authorization was cancelled. Shadowless works normally without input capture.");
    case Result::NoAgent:
        return QStringLiteral("Your desktop has no authentication agent, so the system cannot show its password dialog. "
                              "Start one (Hyprland: hyprpolkitagent; others: polkit-kde-agent, polkit-gnome) or use the manual steps below.");
    case Result::NotAuthorized:
        return QStringLiteral("Your system did not authorize the change (wrong password, or this account is not allowed to administer the system). "
                              "Try again, or ask an administrator. Shadowless works normally without input capture.");
    case Result::PolkitUnavailable:
        return QStringLiteral("The system authorization service (polkit) is not available, so the access cannot be set up here. "
                              "Install or start polkit, or use the manual steps below. Shadowless works normally without input capture.");
    case Result::HelperBlocked:
        return QStringLiteral("Your system would not let the setup tool run (for example a read-only or no-execute mount). "
                              "Use the manual steps below. Shadowless works normally without input capture.");
    case Result::AppliedNeedsReload:
        return QStringLiteral("The change was saved, but your system has not applied it yet. Reconnect the devices, or sign out and back in.");
    case Result::Unavailable: return QStringLiteral("Automatic setup is not available on this system.");
    case Result::Failed: break;
    }
    switch (exitCode) {
    case 2: return QStringLiteral("The setup tool rejected the request, so nothing was changed. This is a bug in Shadowless; the manual steps below still work.");
    case 3: return QStringLiteral("Could not save the access rule (the system rules folder may be read-only). Nothing was changed. Use the manual steps below.");
    default: break;
    }
    Q_UNUSED(output)
    return QStringLiteral("The setup did not finish (error %1). Nothing was changed; Shadowless works normally without input capture.").arg(exitCode);
}

InputAccess::InputAccess(QObject *parent) : QObject(parent), m_ruleFile(kRuleFile) {
    qRegisterMetaType<Result>();
    m_launcher = QStandardPaths::findExecutable("pkexec");
    const QString app = QCoreApplication::applicationDirPath();
    for (const QString &candidate : {app + "/../libexec/shadowless/shadowless-input-setup", app + "/shadowless-input-setup", app + "/../helper/input_setup/shadowless-input-setup",
                                     QString("/usr/libexec/shadowless/shadowless-input-setup"), QString("/usr/lib/shadowless/shadowless-input-setup")})
        if (QFileInfo(candidate).isFile() && QFileInfo(candidate).isExecutable()) { m_helper = QFileInfo(candidate).canonicalFilePath(); break; }
}
InputAccess::~InputAccess() { cleanup(); }

QString InputAccess::helperPath() const { return m_helper; }
bool InputAccess::available() const { return !m_launcher.isEmpty() && !m_helper.isEmpty(); }
bool InputAccess::ruleInstalled() const { return QFileInfo(m_ruleFile).isFile(); }
void InputAccess::configureForTesting(const QString &launcher, const QString &helper, const QString &ruleFile) {
    m_launcher = launcher; m_helper = helper; m_ruleFile = ruleFile;
}

void InputAccess::cleanup() {}

bool InputAccess::rootControlled(const QString &path) { return rootControlledImpl(path); }

void InputAccess::setStagingBaseForTesting(const QString &base) { m_stagingBase = base; }

namespace {
// Runs as root through pkexec when the helper is not root-owned (AppImage, dev builds). Root, not the user, creates the
// directory and file, the helper bytes arrive on stdin, and they must match the hash passed in the (unforgeable) argv.
// A user-controlled copy of the helper is therefore never executed as root, and nothing can be swapped after authorization.
constexpr const char *kRunScript =
    "set -eu; sum=$1; base=$2; shift 2; umask 077; "
    "d=$(mktemp -d \"$base/shadowless-input.XXXXXXXX\"); trap 'rm -rf \"$d\"' EXIT; "
    "cat > \"$d/h\"; printf '%s  %s\\n' \"$sum\" \"$d/h\" | sha256sum -c - >/dev/null; chmod 0700 \"$d/h\"; \"$d/h\" \"$@\"";
} // namespace

void InputAccess::grant(const QVector<AccessDevice> &devices) {
    const QStringList list = triples(devices);
    if (list.isEmpty()) { emit finished(Result::Failed, "No supported device was selected."); return; }
    if (list.size() > 16) { emit finished(Result::Failed, "Too many devices selected (16 at most)."); return; }
    run(QStringList{"install"} + list);
}
void InputAccess::revoke() { run({"remove"}); }

void InputAccess::run(const QStringList &helperArguments) {
    if (m_process) { emit finished(Result::Failed, "A setup is already in progress."); return; }
    if (!available()) { emit finished(Result::Unavailable, "Automatic setup is not available on this system."); return; }
    QStringList arguments;
    QByteArray payload;
    if (rootControlled(m_helper)) {
        arguments = QStringList{m_helper} + helperArguments;
    } else {
        QFile file(m_helper);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) { emit finished(Result::Failed, "Could not read the setup helper."); return; }
        payload = file.readAll();
        const QString shell = QFileInfo("/usr/bin/sh").exists() ? "/usr/bin/sh" : "/bin/sh";
        arguments = QStringList{shell, "-c", kRunScript, "sh", QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex()), m_stagingBase} + helperArguments;
    }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        auto *p = std::exchange(m_process, nullptr); p->deleteLater();
        emit finished(Result::Failed, "The authorization tool could not be started.");
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        auto *p = std::exchange(m_process, nullptr);
        const QString output = QString::fromUtf8(p->readAll()).trimmed().left(600);
        p->deleteLater();
        const Result result = resultForExit(code, status, output);
        m_diagnostics = QStringLiteral("exit code %1, %2\n%3").arg(code).arg(status == QProcess::NormalExit ? "normal exit" : "crashed", output);
        if (result != Result::Granted && result != Result::Cancelled) qWarning().noquote() << "Input access setup:" << m_diagnostics;
        emit finished(result, messageFor(result, code, output));
    });
    m_process->start(m_launcher, arguments);
    if (!payload.isEmpty()) m_process->write(payload);
    m_process->closeWriteChannel();
}

} // namespace Shadowless::Input
