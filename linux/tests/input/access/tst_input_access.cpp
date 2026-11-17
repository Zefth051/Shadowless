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
void node(const QString &sys, const QString &dev, int n, const QString &name, const QString &vendor, const QString &product, const QString &caps, bool readable = true, const QString &ev = "10001f", const QString &rel = "3") {
    const QString base = QString("%1/event%2/device").arg(sys).arg(n);
    put(base + "/name", name + "\n"); put(base + "/id/vendor", vendor + "\n"); put(base + "/id/product", product + "\n");
    put(base + "/capabilities/key", caps + "\n"); put(base + "/capabilities/ev", ev + "\n"); put(base + "/capabilities/rel", rel + "\n");
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
    void groupsInterfacesIntoOnePhysicalDeviceWithTheRightClass() {
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        const QString kb = keyCaps({28, 30, 44, 57}), ms = keyCaps({272, 273});
        // MCHOSE Jet75-II (keyboard with a pointer interface), measured from real sysfs
        node(sys, dev, 10, "MCHOSE Jet75-II", "41e4", "211a", kb, true, "120013", "0");
        node(sys, dev, 11, "MCHOSE Jet75-II", "41e4", "211a", keyCaps({}), true, "9", "0");
        node(sys, dev, 12, "MCHOSE Jet75-II Keyboard", "41e4", "211a", kb, true, "10001f", "1040");
        node(sys, dev, 13, "MCHOSE Jet75-II Mouse", "41e4", "211a", ms, true, "17", "1943");
        node(sys, dev, 14, "MCHOSE Jet75-II Wireless Radio Control", "41e4", "211a", keyCaps({247}), true, "13", "0");
        // Compx Wireless mouse 8K (mouse whose dongle also exposes a keyboard interface)
        node(sys, dev, 16, "Compx Wireless mouse 8k dongle", "373b", "111a", ms, true, "17", "1943");
        node(sys, dev, 19, "Compx Wireless mouse 8k dongle Consumer Control", "373b", "111a", kb, true, "1f", "1040");  // no autorepeat: not a keyboard
        node(sys, dev, 22, "Compx Wireless mouse 8k dongle Mouse", "373b", "111a", ms, true, "17", "1943");
        node(sys, dev, 23, "Compx Wireless mouse 8k dongle", "373b", "111a", kb, true, "120013", "0");
        // DualSense: pad wins; its touchpad (no relative axes) is never a mouse
        node(sys, dev, 30, "Wireless Controller", "054c", "0ce6", keyCaps({304, 305}), true, "1b", "0");
        node(sys, dev, 31, "Wireless Controller Touchpad", "054c", "0ce6", keyCaps({272}), true, "b", "0");
        const auto groups = InputAccess::groupDevices(InputAccess::enumerate(sys, dev));
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups[0].triple(), QString("41e4:211a:keyboard"));
        QCOMPARE(groups[0].name, QString("MCHOSE Jet75-II"));
        QVERIFY(groups[0].alternatives.isEmpty());                       // confident: no category question for the user
        QCOMPARE(groups[1].triple(), QString("373b:111a:mouse"));
        QCOMPARE(groups[1].name, QString("Compx Wireless mouse 8k dongle"));
        QVERIFY(groups[1].alternatives.isEmpty());
        QCOMPARE(groups[2].triple(), QString("054c:0ce6:gamepad"));
        QCOMPARE(InputAccess::triples(groups).size(), 3);                // exactly one grant per physical device
    }
    void otherMachinesAreClassifiedFromCapabilitiesNotFromKnownNames() {
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        const QString kb = keyCaps({28, 30, 44, 57}), ms = keyCaps({272, 273});
        // Laptop: built-in keyboard (i8042), touchpad (absolute axes), trackpoint (relative axes), lid switch
        node(sys, dev, 3, "AT Translated Set 2 keyboard", "0001", "0001", kb, true, "120013", "0");
        node(sys, dev, 5, "SynPS/2 Synaptics TouchPad", "0002", "0007", ms, true, "b", "0");
        node(sys, dev, 6, "TPPS/2 IBM TrackPoint", "0002", "000a", ms, true, "7", "3");
        // Bluetooth keyboard and a gaming mouse that exposes extra macro interfaces
        node(sys, dev, 7, "Keychron K8 Pro", "05ac", "024f", kb, true, "120013", "0");
        node(sys, dev, 8, "Razer Viper Ultimate", "1532", "0078", ms, true, "17", "1943");
        node(sys, dev, 9, "Razer Viper Ultimate Keyboard", "1532", "0078", kb, true, "120013", "0");
        node(sys, dev, 10, "Razer Viper Ultimate", "1532", "0078", ms, true, "17", "1943");
        // Xbox pad plus a virtual device without ids (uinput) that must never be granted
        node(sys, dev, 11, "Xbox Wireless Controller", "045e", "0b13", keyCaps({304, 305}), true, "b", "0");
        node(sys, dev, 12, "virtual-keyboard", "0000", "0000", kb, true, "120013", "0");
        const auto groups = InputAccess::groupDevices(InputAccess::enumerate(sys, dev));
        QStringList triples;
        for (const auto &g : groups) triples << g.triple();
        triples.sort();
        QCOMPARE(triples, (QStringList{"0001:0001:keyboard", "0002:000a:mouse", "045e:0b13:gamepad", "05ac:024f:keyboard", "1532:0078:mouse"}));
        for (const auto &g : groups) if (g.vendor == "1532") QVERIFY(g.alternatives.isEmpty());   // two pointer nodes outweigh one keyboard interface: no question asked
    }
    void trueHybridsKeepTheirAlternativeAvailable() {
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        node(sys, dev, 1, "Acme Combo", "1234", "abcd", keyCaps({28, 30, 44}), true, "120013", "0");
        node(sys, dev, 2, "Acme Combo Pointer", "1234", "abcd", keyCaps({272}), true, "17", "3");
        const auto groups = InputAccess::groupDevices(InputAccess::enumerate(sys, dev));
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].triple(), QString("1234:abcd:keyboard"));
        QCOMPARE(groups[0].alternatives, (QVector<AccessDevice::Class>{AccessDevice::Mouse}));
        QCOMPARE(groups[0].asClass(AccessDevice::Mouse).triple(), QString("1234:abcd:mouse"));
    }
    void groupReadabilityRequiresEveryNodeOfTheChosenClass() {
        if (::geteuid() == 0) QSKIP("root can read every node, so permission checks are meaningless here");
        QTemporaryDir t; const QString sys = t.filePath("sys"), dev = t.filePath("dev");
        node(sys, dev, 16, "Some Mouse", "373b", "111a", keyCaps({272}), true);
        node(sys, dev, 22, "Some Mouse", "373b", "111a", keyCaps({272}), false);
        QVERIFY(!InputAccess::groupDevices(InputAccess::enumerate(sys, dev))[0].readable);
    }
    void grantStreamsAHashCheckedHelperToRootInsteadOfRunningAUserCopy() {
        QTemporaryDir t;
        const QString log = t.filePath("args.log"), out = t.filePath("ran.log");
        const QString launcher = script(t.path(), "pkexec", "printf '%s\\n' \"$@\" > " + log + "\nexec \"$@\"");   // stands in for pkexec
        const QString helper = script(t.path(), "helper", "echo \"$@\" > " + out + "\necho '{\"ok\":true}'");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules")); access.setStagingBaseForTesting(t.path());
        AccessDevice good; good.vendor = "41e4"; good.product = "211a"; good.cls = AccessDevice::Keyboard;
        AccessDevice evil; evil.vendor = "41e4;x"; evil.product = "211a"; evil.cls = AccessDevice::Mouse;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({good, evil});
        QVERIFY(access.busy());
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Granted);
        QFile f(log); QVERIFY(f.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
        QVERIFY(lines[0] == "/usr/bin/sh" || lines[0] == "/bin/sh");                 // a root-owned interpreter, never the user's copy
        QCOMPARE(lines[1], QString("-c"));
        QVERIFY(!lines.contains(helper));                                          // the helper path is not what gets authorized
        QCOMPARE(lines[4].size(), 64);                                             // sha256 of the helper, fixed in argv
        QCOMPARE(lines[6], QString("install")); QCOMPARE(lines[7], QString("41e4:211a:keyboard"));
        QCOMPARE(lines.size(), 9);                                                // trailing empty entry only: the evil device never reached the command line
        QCOMPARE(QString::fromUtf8(QFile(out).open(QIODevice::ReadOnly) ? QByteArray("ok") : QByteArray()), QString("ok"));
        QCOMPARE(QDir(t.path()).entryList({"shadowless-input.*"}, QDir::Dirs).size(), 0);   // root's private directory is removed afterwards
    }
    void aTamperedHelperStreamIsRejectedBeforeItRuns() {
        QTemporaryDir t;
        const QString out = t.filePath("ran.log");
        const QString launcher = script(t.path(), "pkexec", "echo '#!/bin/sh' > " + out + "\nprintf '#!/bin/sh\\ntouch " + out + ".pwned\\n' | exec \"$@\"");   // substitutes the bytes in transit
        const QString helper = script(t.path(), "helper", "exit 0");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules")); access.setStagingBaseForTesting(t.path());
        AccessDevice d; d.vendor = "41e4"; d.product = "211a"; d.cls = AccessDevice::Keyboard;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({d});
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Failed);
        QVERIFY(!QFileInfo::exists(out + ".pwned"));                                // the substituted program never executed
    }
    void onlyRootControlledHelpersRunInPlace() {
        QTemporaryDir t;
        QVERIFY(!InputAccess::rootControlled(script(t.path(), "helper", "exit 0")));  // user-writable: must be staged, never trusted by path
        QVERIFY(!InputAccess::rootControlled("/nonexistent/helper"));
        if (QFileInfo("/usr/bin/env").ownerId() == 0) QVERIFY(InputAccess::rootControlled("/usr/bin/env"));  // not meaningful inside a user namespace
        const QString link = t.filePath("link"); QVERIFY(QFile::link("/usr/bin/env", link));
        QVERIFY(!InputAccess::rootControlled(link));                                  // symlinks are never trusted
    }
    void dismissedDialogIsNotAnError() {
        QTemporaryDir t;
        const QString launcher = script(t.path(), "pkexec", "exit 126");
        const QString helper = script(t.path(), "helper", "exit 0");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules")); access.setStagingBaseForTesting(t.path());
        AccessDevice d; d.vendor = "1111"; d.product = "2222"; d.cls = AccessDevice::Mouse;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({d});
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::Cancelled);
        QVERIFY(done[0][1].toString().contains("works normally"));
    }
    void missingAuthenticationAgentIsReportedClearly() {
        QTemporaryDir t;
        const QString launcher = script(t.path(), "pkexec", "echo 'Error executing command as another user: No authentication agent found.' >&2; exit 127");
        const QString helper = script(t.path(), "helper", "exit 0");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules")); access.setStagingBaseForTesting(t.path());
        AccessDevice d; d.vendor = "1111"; d.product = "2222"; d.cls = AccessDevice::Mouse;
        QSignalSpy done(&access, &InputAccess::finished);
        access.grant({d});
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        QCOMPARE(done[0][0].value<InputAccess::Result>(), InputAccess::Result::NoAgent);
        QVERIFY(done[0][1].toString().contains("hyprpolkitagent"));
    }
    void helperFailureSurfacesItsMessageAndRecovers() {
        QTemporaryDir t;
        const QString launcher = script(t.path(), "pkexec", "exec \"$@\"");
        const QString helper = script(t.path(), "helper", "echo 'udev reload failed'; exit 4");
        InputAccess access; access.configureForTesting(launcher, helper, t.filePath("rules")); access.setStagingBaseForTesting(t.path());
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
        InputAccess access; access.configureForTesting(launcher, helper, rules); access.setStagingBaseForTesting(t.path());
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
