#include "input_access.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <unistd.h>

namespace Shadowless::Input {
namespace {
constexpr const char *kRuleFile = "/etc/udev/rules.d/70-shadowless-input.rules";
constexpr int kKeyA = 30, kKeyZ = 44, kKeyEnter = 28, kBtnLeft = 272, kBtnSouth = 304, kBtnEast = 305;

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
        if (hasKey(keys, kBtnSouth) || hasKey(keys, kBtnEast)) device.cls = AccessDevice::Gamepad;
        else if (hasKey(keys, kKeyA) && hasKey(keys, kKeyZ) && hasKey(keys, kKeyEnter)) device.cls = AccessDevice::Keyboard;
        else if (hasKey(keys, kBtnLeft)) device.cls = AccessDevice::Mouse;
        else continue;
        if (!validTriple(device.triple())) continue;               // virtual or oddly identified devices cannot be granted
        device.readable = ::access(QFile::encodeName(device.node).constData(), R_OK) == 0;
        devices.append(device);
    }
    return devices;
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

InputAccess::Result InputAccess::resultForExit(int exitCode, QProcess::ExitStatus status) {
    if (status != QProcess::NormalExit) return Result::Failed;
    if (exitCode == 0) return Result::Granted;
    if (exitCode == 126 || exitCode == 127) return Result::Cancelled; // pkexec: dialog dismissed or not authorized
    return Result::Failed;
}

InputAccess::InputAccess(QObject *parent) : QObject(parent), m_ruleFile(kRuleFile) {
    qRegisterMetaType<Result>();
    m_launcher = QStandardPaths::findExecutable("pkexec");
    const QString app = QCoreApplication::applicationDirPath();
    for (const QString &candidate : {app + "/../libexec/shadowless/shadowless-input-setup", app + "/shadowless-input-setup",
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

void InputAccess::cleanup() {
    if (!m_staged.isEmpty()) { QDir(QFileInfo(m_staged).absolutePath()).removeRecursively(); m_staged.clear(); }
}

// A helper inside a FUSE-mounted AppImage is not reachable by root, so it is copied to a private 0700 directory first.
// Helpers installed in system directories are run in place.
QString InputAccess::stagedHelper(QString *error) {
    if (m_helper.startsWith("/usr/") || m_helper.startsWith("/opt/")) return m_helper;
    const QString dir = QDir::temp().filePath("shadowless-setup-" + QUuid::createUuid().toString(QUuid::Id128).left(12));
    if (!QDir().mkpath(dir) || !QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) { *error = "Could not prepare a private directory for the setup helper."; return {}; }
    const QString target = dir + "/shadowless-input-setup";
    if (!QFile::copy(m_helper, target) || !QFile::setPermissions(target, QFile::ReadOwner | QFile::ExeOwner)) { *error = "Could not stage the setup helper."; QDir(dir).removeRecursively(); return {}; }
    m_staged = target;
    return target;
}

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
    QString error;
    const QString helper = stagedHelper(&error);
    if (helper.isEmpty()) { emit finished(Result::Failed, error); return; }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        auto *p = std::exchange(m_process, nullptr); p->deleteLater(); cleanup();
        emit finished(Result::Failed, "The authorization tool could not be started.");
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        auto *p = std::exchange(m_process, nullptr);
        const QString output = QString::fromUtf8(p->readAll()).trimmed().left(300);
        p->deleteLater(); cleanup();
        const Result result = resultForExit(code, status);
        emit finished(result, result == Result::Granted ? QString("Input access enabled.")
                              : result == Result::Cancelled ? QString("Authorization was cancelled. Shadowless works normally without input capture.")
                              : output.isEmpty() ? QString("The setup failed (exit code %1).").arg(code) : output);
    });
    m_process->start(m_launcher, QStringList{helper} + helperArguments);
}

} // namespace Shadowless::Input
