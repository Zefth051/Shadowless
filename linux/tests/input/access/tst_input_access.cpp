#include "input_access.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <unistd.h>

using namespace Shadowless::Input;

namespace {
void put(const QString &path, const QString &text) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(text.toUtf8());
}
// key capability words, highest first: bit positions set among the first 64 / second 64 codes
QString keyCaps(std::initializer_list<int> codes) {
    quint64 w[6] = {};
    for (int c : codes) w[c / 64] |= quint64(1) << (c % 64);
    QStringList words;
    for (int i = 5; i >= 0; --i) words << QString::number(w[i], 16);
    while (words.size() > 1 && words.first() == "0") words.removeFirst();
    return words.join(' ');
}
void node(const QString &sys, const QString &dev, int n, const QString &name, const QString &vendor, const QString &product, const QString &caps, bool readable = true) {
    const QString base = QString("%1/event%2/device").arg(sys).arg(n);
    put(base + "/name", name + "\n"); put(base + "/id/vendor", vendor + "\n"); put(base + "/id/product", product + "\n");
    put(base + "/capabilities/key", caps + "\n");
    put(QString("%1/event%2").arg(dev).arg(n), "");
    if (!readable) QFile::setPermissions(QString("%1/event%2").arg(dev).arg(n), {});
}
} // namespace

class AccessTests : public QObject {
    Q_OBJECT
    QString script(const QString &dir, const QString &name, const QString &body) {
        const QString path = dir + "/" + name;
        put(path, "#!/bin/sh\n" + body + "\n");
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }
private slots:
    void classifiesNodesAndSkipsEverythingElse() {
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        node(sys, dev, 10, "MCHOSE Jet75-II", "41E4", "211A", keyCaps({28, 30, 44, 57}));        // keyboard, uppercase hex
        node(sys, dev, 16, "Compx mouse", "373b", "111a", keyCaps({272, 273}));                  // mouse
        node(sys, dev, 17, "Pad", "054c", "0ce6", keyCaps({304, 305}));                          // gamepad (wins over others)
        node(sys, dev, 0, "Power Button", "0000", "0001", keyCaps({116}));                      // not an input we use
        node(sys, dev, 5, "Evil;name", "zz;z", "0001", keyCaps({28, 30, 44}));                   // malformed ids: unusable
        const auto d = InputAccess::enumerate(sys, dev);
        QCOMPARE(d.size(), 3);
        QCOMPARE(d[0].triple(), QString("41e4:211a:keyboard"));
        QCOMPARE(d[1].triple(), QString("373b:111a:mouse"));
        QCOMPARE(d[2].triple(), QString("054c:0ce6:gamepad"));
        QCOMPARE(InputAccess::triples(d), (QStringList{"054c:0ce6:gamepad", "373b:111a:mouse", "41e4:211a:keyboard"}));
    }
    void detectsReadablePermissionsWithoutOpeningNodes() {
        if (::geteuid() == 0) QSKIP("root can read everything");
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        node(sys, dev, 1, "Keyboard", "1111", "2222", keyCaps({28, 30, 44})); node(sys, dev, 2, "Mouse", "3333", "4444", keyCaps({272}), false);
        const auto d = InputAccess::enumerate(sys, dev);
        QVERIFY(d[0].readable); QVERIFY(!d[1].readable);
        QVERIFY(!InputAccess::allReadable(d)); QVERIFY(InputAccess::allReadable({d[0]}));
        QVERIFY(InputAccess::allReadable({}));
        QFile::setPermissions(dev + "/event2", QFile::ReadOwner | QFile::WriteOwner);
    }
    void primaryNodePerPhysicalDevice() {
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        node(sys, dev, 23, "Dongle kbd", "373b", "111a", keyCaps({28, 30, 44}));
        node(sys, dev, 16, "Dongle", "373b", "111a", keyCaps({272}));
        node(sys, dev, 22, "Dongle", "373b", "111a", keyCaps({272}));
        const auto p = InputAccess::primaryNodes(InputAccess::enumerate(sys, dev));
        QCOMPARE(p.size(), 1); QCOMPARE(p[0].eventNumber, 16); QCOMPARE(p[0].cls, AccessDevice::Mouse);
    }
    void triplesRejectInjection() {
        for (const QString bad : {"41e4:211a:keyboard;rm", "41E4:211a:keyboard", "41e4:211a:KEYBOARD", "41e4:211a:", "41e4:211a:keyboard\n", "41e4 211a keyboard", "../..:1111:mouse", "41e4:211a:mouse -x"})
            QVERIFY2(!InputAccess::validTriple(bad), qPrintable(bad));
        QVERIFY(InputAccess::validTriple("41e4:211a:keyboard"));
    }
    void exitCodesMapToUserOutcomes() {
        using R = InputAccess::Result;
        QCOMPARE(InputAccess::resultForExit(0, QProcess::NormalExit), R::Granted);
        QCOMPARE(InputAccess::resultForExit(126, QProcess::NormalExit), R::Cancelled);   // dialog dismissed
        QCOMPARE(InputAccess::resultForExit(127, QProcess::NormalExit), R::Cancelled);   // not authorized
        QCOMPARE(InputAccess::resultForExit(2, QProcess::NormalExit), R::Failed);
        QCOMPARE(InputAccess::resultForExit(4, QProcess::NormalExit), R::Failed);
        QCOMPARE(InputAccess::resultForExit(0, QProcess::CrashExit), R::Failed);
    }
    void grantRunsTheHelperThroughTheLauncherWithOnlyValidatedArguments() {
        QTemporaryDir t;
        const QString log = t.filePath("args.log");
        const QString launcher = script(t.path(), "pkexec", "printf '%s\\n' \"$@\" > " + log + "\nexec \"$@\"");
        const QString helper = script(t.path(), "helper", "echo '{\"ok\":true}'");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules"));
        QVERIFY(access.available());
        AccessDevice good; good.vendor = "41e4"; good.product = "211a"; good.cls = AccessDevice::Keyboard;
        AccessDevice evil; evil.vendor = "41e4;x"; evil.product = "211a"; evil.cls = AccessDevice::Mouse;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({good, evil});
        QVERIFY(access.busy());
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Granted);
        QFile f(log); QVERIFY(f.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);                                       // staged helper, "install", exactly one triple
        QVERIFY(lines[0] != helper);                                     // not run from the original (possibly FUSE) location
        QCOMPARE(lines[1], QString("install")); QCOMPARE(lines[2], QString("41e4:211a:keyboard"));
        QVERIFY(!QFileInfo::exists(lines[0]));                           // staged copy removed afterwards
        QVERIFY(!access.busy());
    }
    void dismissedDialogIsNotAnError() {
        QTemporaryDir t;
        const QString launcher = script(t.path(), "pkexec", "exit 126");
        const QString helper = script(t.path(), "helper", "exit 0");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules"));
        AccessDevice d; d.vendor = "1111"; d.product = "2222"; d.cls = AccessDevice::Mouse;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({d});
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Cancelled);
        QVERIFY(done[0][1].toString().contains("works normally"));
    }
    void helperFailureSurfacesItsMessageAndRecovers() {
        QTemporaryDir t;
        const QString launcher = script(t.path(), "pkexec", "exec \"$@\"");
        const QString helper = script(t.path(), "helper", "echo 'udev reload failed'; exit 4");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules"));
        AccessDevice d; d.vendor = "1111"; d.product = "2222"; d.cls = AccessDevice::Mouse;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({d});
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Failed);
        QVERIFY(done[0][1].toString().contains("udev reload failed"));
        QVERIFY(!access.busy());
        access.grant({d});                                               // can try again after a failure
        QVERIFY(access.busy());
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 10000);
    }
    void unavailableWithoutLauncherOrHelperAndNothingSelected() {
        InputAccess access; access.configureForTesting({}, {}, "/nonexistent/rules");
        QVERIFY(!access.available()); QVERIFY(!access.ruleInstalled());
        QSignalSpy done(&access, &InputAccess::finished);
        AccessDevice d; d.vendor = "1111"; d.product = "2222"; d.cls = AccessDevice::Mouse;
        access.grant({d});
        QCOMPARE(done.size(), 1); QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Unavailable);
        access.grant({});
        QCOMPARE(done.size(), 2); QCOMPARE(done[1][0].value<InputAccess::Result>(), InputAccess::Result::Failed);
    }
    void revokeUsesTheSameHelperAndRuleDetection() {
        QTemporaryDir t;
        const QString log = t.filePath("args.log"), rules = t.filePath("70.rules");
        put(rules, "# rule\n");
        const QString launcher = script(t.path(), "pkexec", "printf '%s\\n' \"$@\" > " + log + "\nexit 0");
        const QString helper = script(t.path(), "helper", "exit 0");
        InputAccess access; access.configureForTesting(launcher, helper, rules);
        QVERIFY(access.ruleInstalled());
        QSignalSpy done(&access, &InputAccess::finished);
        access.revoke();
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QFile f(log); QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(f.readAll()).trimmed().endsWith("\nremove"));
    }
};
QTEST_MAIN(AccessTests)
#include "tst_input_access.moc"
