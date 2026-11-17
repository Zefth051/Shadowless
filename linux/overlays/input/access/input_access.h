#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Shadowless::Input {

// One input node as seen in sysfs (name, USB/BT ids, class from key capabilities, readable by this user).
struct AccessDevice {
    enum Class { Keyboard, Mouse, Gamepad };
    QString name, vendor, product, node;
    Class cls = Keyboard;
    int eventNumber = 0;
    bool readable = false;
    QString identity() const { return vendor + ":" + product; }                  // groups nodes of one physical device
    QVector<AccessDevice> members;                                               // group only: every classified node of the physical device
    QVector<Class> alternatives;                                                 // group only: other classes with verified nodes (hybrid devices)
    AccessDevice asClass(Class other) const;                                     // group only: same device seen as another verified class
    QString triple() const;                                                      // "41e4:211a:keyboard", the helper's argument
};

// Prepares and runs the one-time, user-authorized permission setup. It never runs as root itself:
// the elevated work is done by the small shadowless-input-setup helper through pkexec, and
// declining the system dialog leaves Shadowless fully working without input capture.
class InputAccess : public QObject {
    Q_OBJECT
public:
    // Cancelled: the user dismissed the system dialog (the only case that is not a problem).
    // NoAgent: no polkit authentication agent runs in this session. NotAuthorized: authentication was refused or the user is
    // not allowed. PolkitUnavailable: polkit/pkexec itself is missing or broken. HelperBlocked: the system would not run the
    // setup tool (for example a noexec mount). AppliedNeedsReload: the access rule was written but udev did not apply it yet.
    enum class Result { Granted, Cancelled, Failed, Unavailable, NoAgent, NotAuthorized, PolkitUnavailable, HelperBlocked, AppliedNeedsReload };
    Q_ENUM(Result)
    explicit InputAccess(QObject *parent = nullptr);
    ~InputAccess() override;

    // Reads /sys/class/input only (no device node is opened). Nodes that are neither keyboard, mouse nor gamepad are skipped.
    static QVector<AccessDevice> enumerate(const QString &sysRoot = "/sys/class/input", const QString &devRoot = "/dev/input");
    // One entry per physical device (vendor:product) with its most likely class decided automatically from verified
    // capabilities and the device's own name. Extra HID interfaces (vendor, consumer, radio control) are folded in.
    static QVector<AccessDevice> groupDevices(const QVector<AccessDevice> &nodes);
    // Triples for the devices to grant: unique, valid, sorted; hostile or malformed ids are dropped.
    static QStringList triples(const QVector<AccessDevice> &devices);
    // The node of each physical device that the user most likely means (lowest event number) and its siblings.
    static QVector<AccessDevice> primaryNodes(const QVector<AccessDevice> &devices);
    static bool allReadable(const QVector<AccessDevice> &devices);
    static bool validTriple(const QString &triple);
    // True when root controls the file and all its parent directories (safe to authorize in place).
    static bool rootControlled(const QString &path);
    // pkexec exits 126 when the dialog is dismissed and 127 for every other refusal or error, so the merged stdout/stderr is
    // needed to tell them apart. Helper exit codes: 2 invalid request, 3 I/O error, 4 rule written but udev reload failed.
    static Result resultForExit(int exitCode, QProcess::ExitStatus status, const QString &output = QString());
    static QString messageFor(Result result, int exitCode, const QString &output);

    QString helperPath() const;                  // empty when no helper ships with this build
    bool available() const;                      // pkexec and a helper are both present
    bool ruleInstalled() const;
    void setStagingBaseForTesting(const QString &base);
    void configureForTesting(const QString &launcher, const QString &helper, const QString &ruleFile);

    void grant(const QVector<AccessDevice> &devices);   // asynchronous; emits finished()
    void revoke();                                       // removes the rule through the same helper
    bool busy() const { return m_process != nullptr; }
    QString diagnostics() const { return m_diagnostics; }   // technical detail of the last run (exit code and tool output) for support, not for the main message

signals:
    void finished(Shadowless::Input::InputAccess::Result result, const QString &message);

private:
    QString m_launcher, m_helper, m_ruleFile, m_diagnostics;
    QProcess *m_process = nullptr;
    QString m_stagingBase = QStringLiteral("/run");
    void run(const QStringList &helperArguments);
    void cleanup();
};

} // namespace Shadowless::Input
Q_DECLARE_METATYPE(Shadowless::Input::InputAccess::Result)
