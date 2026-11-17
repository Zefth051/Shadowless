#include "../../helper/input_setup/device.h"
#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <sys/stat.h>
#include <unistd.h>

class HelperTests : public QObject {
    Q_OBJECT
    QString helper = qEnvironmentVariable("INPUT_SETUP_HELPER");
    QByteArray output;
    int run(const QStringList &args, bool pkexec = false) {
        QProcess p;
        auto env = QProcessEnvironment::systemEnvironment();
        env.remove("PKEXEC_UID");
        if (pkexec) env.insert("PKEXEC_UID", "1000");
        p.setProcessEnvironment(env);
        const auto rootIndex = args.indexOf("--root");
        if (rootIndex >= 0 && rootIndex + 1 < args.size() && QFileInfo(args[rootIndex + 1]).isDir())
            p.setWorkingDirectory(args[rootIndex + 1]);
        p.start(helper, args);
        if (!p.waitForFinished(10000)) return -1;
        output = p.readAllStandardOutput();
        if (p.exitStatus() != QProcess::NormalExit || !output.endsWith('\n') ||
            output.count('\n') != 1 || !QJsonDocument::fromJson(output).isObject()) return -1;
        const auto json = QJsonDocument::fromJson(output).object();
        const bool ok = p.exitCode() == 0;
        if (json.value("ok") != QJsonValue(ok) || !json.value("installed").isBool() ||
            (ok ? json.contains("error") : json.value("error").toString().isEmpty()) ||
            (ok && args.contains("status") && !json.value("devices").isArray())) return -1;
        return p.exitCode();
    }
    static QByteArray read(const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return f.readAll();
    }
    static QString rules(const QTemporaryDir &dir) {
        return dir.path() + "/etc/udev/rules.d/71-shadowless-input-access.rules";
    }
    static void put(const QString &path, const QByteArray &data) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(data);
    }
private slots:
    void removeRevokesOnlyTheUsersAclOnUntaggedMatchingNodes() {
        QTemporaryDir dir;
        const QString r = dir.path();
        const QStringList base = {"--root", r, "--no-reload"};
        QCOMPARE(run(base + QStringList{"install", "41e4:211a:keyboard", "373b:111a:mouse"}), 0);
        auto node = [&](const char *n, const char *v, const char *p, const char *mm) {
            put(r + "/sys/class/input/" + n + "/device/id/vendor", QByteArray(v) + "\n");
            put(r + "/sys/class/input/" + n + "/device/id/product", QByteArray(p) + "\n");
            put(r + "/sys/class/input/" + n + "/dev", QByteArray(mm) + "\n");
        };
        node("event10", "41e4", "211a", "13:74");   // matches, no longer tagged -> revoked
        node("event12", "373b", "111a", "13:76");   // matches but still tagged by another rule -> kept
        node("event3",  "046d", "c548", "13:67");   // unrelated device -> never touched
        node("event../x", "41e4", "211a", "13:99");  // malformed name -> ignored
        put(r + "/run/udev/data/c13:74", "I:1\nQ:seat\n");
        put(r + "/run/udev/data/c13:76", "I:1\nQ:uaccess\n");
        const QString log = r + "/setfacl.log";
        put(r + "/usr/bin/setfacl", "#!/bin/sh\necho \"$@\" >> " + log.toUtf8() + "\n");
        QFile::setPermissions(r + "/usr/bin/setfacl", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        qputenv("INPUT_SETUP_TEST_UID", "1000");
        QCOMPARE(run(base + QStringList{"remove"}), 0);
        qunsetenv("INPUT_SETUP_TEST_UID");
        QVERIFY(!QFileInfo::exists(rules(dir)));
        QCOMPARE(read(log).trimmed(), QByteArray("-x u:1000 ") + r.toUtf8() + "/dev/input/event10");
        QFile::remove(log);
        QCOMPARE(run(base + QStringList{"install", "41e4:211a:keyboard"}), 0);
        qputenv("INPUT_SETUP_TEST_UID", "1000; rm -rf /");   // malformed uid is ignored
        QCOMPARE(run(base + QStringList{"remove"}), 0);
        qunsetenv("INPUT_SETUP_TEST_UID");
        QVERIFY(!QFileInfo::exists(log));
    }
    void initTestCase() { QVERIFY2(QFileInfo::exists(helper), "Set INPUT_SETUP_HELPER to the built helper"); }
    void validation() {
        QTemporaryDir dir;
        const QStringList base = {"--root", dir.path(), "--no-reload"};
        for (const auto &device : QStringList{"046d:c548:keyboard;id", "41e4:211a:keyboard\n", " 41e4:211a:keyboard", "41e4:211a:keyboard ", "41e4:211A:mouse", "046D:c548:keyboard", "46d:c548:keyboard", "046d:c54:mouse", "046d:c548:joystick", "046d:c548:$(id)", "046d:c548:mouse\"", "046d:c548:mouse:extra"}) {
            QCOMPARE(run(base + QStringList{"install", device}), 2);
        }
        QCOMPARE(run(base + QStringList{"unknown"}), 2);
        QCOMPARE(run(base + QStringList{"install"}), 2);
        QCOMPARE(run(base + QStringList{"status", "046d:c548:mouse"}), 2);
        QCOMPARE(run(base + QStringList{"remove", "046d:c548:mouse"}), 2);
        QStringList many;
        for (int i = 0; i < 17; ++i) many << "046d:c548:mouse";
        QCOMPARE(run(base + QStringList{"install"} + many), 2);
        many.removeLast();
        QCOMPARE(run(base + QStringList{"install"} + many), 0);
        QVERIFY(!read(rules(dir)).isEmpty());
        QCOMPARE(run(base + QStringList{"status"}, true), 2);
        QCOMPARE(run({"--root"}), 2);
        QCOMPARE(run(base + QStringList{"--bad", "status"}), 2);
        QCOMPARE(run({}), 2);
    }
    void embeddedNul() {
        // execve cannot transport an embedded NUL. Exercise the same argument parser directly.
        using namespace std::string_literals;
        input_setup::Device parsed;
        QVERIFY(!input_setup::parseDevice("41e4:211a:keyboard\0"s, parsed));
        QVERIFY(!input_setup::parseDevice("41e4:211a:key\0board"s, parsed));
    }
    void lifecycle() {
        QTemporaryDir dir;
        const QStringList base = {"--root", dir.path(), "--no-reload"};
        QCOMPARE(run(base + QStringList{"status"}), 0);
        QCOMPARE(QJsonDocument::fromJson(output).object()["devices"], QJsonValue(QJsonArray{}));
        QVERIFY(!QJsonDocument::fromJson(output).object()["installed"].toBool());
        QCOMPARE(run(base + QStringList{"install", "054c:0ce6:gamepad", "046d:c548:mouse", "046d:c548:keyboard", "046d:c548:mouse"}), 0);
        const QByteArray golden = "# Generated by shadowless-input-setup. Do not edit.\n"
            "SUBSYSTEM==\"input\", KERNEL==\"event*\", ATTRS{id/vendor}==\"046d\", ATTRS{id/product}==\"c548\", ENV{ID_INPUT_KEY}==\"1\", TAG+=\"uaccess\"\n"
            "SUBSYSTEM==\"input\", KERNEL==\"event*\", ATTRS{id/vendor}==\"046d\", ATTRS{id/product}==\"c548\", ENV{ID_INPUT_MOUSE}==\"1\", TAG+=\"uaccess\"\n"
            "SUBSYSTEM==\"input\", KERNEL==\"event*\", ATTRS{id/vendor}==\"054c\", ATTRS{id/product}==\"0ce6\", ENV{ID_INPUT_JOYSTICK}==\"1\", TAG+=\"uaccess\"\n";
        QCOMPARE(read(rules(dir)), golden);
        struct stat st{};
        QVERIFY(::stat(qPrintable(rules(dir)), &st) == 0);
        QCOMPARE(st.st_mode & 0777, mode_t(0644));
        QCOMPARE(run(base + QStringList{"status"}), 0);
        QCOMPARE(QJsonDocument::fromJson(output).object()["devices"],
                 QJsonValue(QJsonArray({"046d:c548:keyboard", "046d:c548:mouse", "054c:0ce6:gamepad"})));
        QVERIFY(QJsonDocument::fromJson(output).object()["installed"].toBool());
        QCOMPARE(run(base + QStringList{"install", "046d:c548:keyboard", "054c:0ce6:gamepad", "046d:c548:mouse"}), 0);
        QCOMPARE(read(rules(dir)), golden);
        QFile old(rules(dir));
        QVERIFY(old.open(QIODevice::ReadOnly));
        QCOMPARE(run(base + QStringList{"install", "1234:abcd:mouse"}), 0);
        QCOMPARE(old.readAll(), golden); // Replacement must preserve already-open old inodes.
        QVERIFY(read(rules(dir)).contains("1234"));
        QVERIFY(!read(rules(dir)).contains("046d"));
        QCOMPARE(QDir(QFileInfo(rules(dir)).path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size(), 1);
        QCOMPARE(run(base + QStringList{"remove"}), 0);
        QVERIFY(!QFileInfo::exists(rules(dir)));
        QCOMPARE(run(base + QStringList{"remove"}), 0);
    }
    void reloadCommands() {
        const QString fixture = qEnvironmentVariable("INPUT_SETUP_RELOAD_HELPER");
        QVERIFY2(QFileInfo::exists(fixture), "Set INPUT_SETUP_RELOAD_HELPER to the reload fixture");
        QScopedValueRollback<QString> binary(helper, fixture);
        QTemporaryDir dir;
        const QStringList base = {"--root", dir.path()};
        const QByteArray commands = "udevadm control --reload-rules\nudevadm trigger --action=change --subsystem-match=input\n";
        const QString log = dir.path() + "/udev.log";
        QCOMPARE(run(base + QStringList{"install", "1234:abcd:mouse", "1234:0001:keyboard"}), 0);
        QCOMPARE(read(log), commands);
        QVERIFY(QFile::remove(log));
        QCOMPARE(run(base + QStringList{"install", "046d:c548:keyboard"}), 0);
        QCOMPARE(read(log), commands);
        QVERIFY(QFile::remove(log));
        QCOMPARE(run(base + QStringList{"status"}), 0);
        QVERIFY(!QFileInfo::exists(log));
        QCOMPARE(run(base + QStringList{"remove"}), 0);
        QCOMPARE(read(log), commands);
        QVERIFY(QFile::remove(log));
        QCOMPARE(run(base + QStringList{"install", "--no-reload", "ffff:0001:mouse"}), 0);
        QVERIFY(!QFileInfo::exists(log));
        QFile triggerFailure(dir.path() + "/fail-trigger");
        QVERIFY(triggerFailure.open(QIODevice::WriteOnly));
        triggerFailure.close();
        QCOMPARE(run(base + QStringList{"install", "ffff:0001:mouse"}), 4);
        QVERIFY(QJsonDocument::fromJson(output).object()["installed"].toBool());
        QVERIFY(read(rules(dir)).contains("ffff"));
        QFile fail(dir.path() + "/fail-reload");
        QVERIFY(fail.open(QIODevice::WriteOnly)); fail.close();
        QCOMPARE(run(base + QStringList{"remove"}), 4);
        QVERIFY(!QJsonDocument::fromJson(output).object()["installed"].toBool());
        QVERIFY(!QFileInfo::exists(rules(dir)));
        QVERIFY(fail.remove());
        QVERIFY(triggerFailure.remove());
        QVERIFY(QFile::remove(log));
        QCOMPARE(run(base + QStringList{"remove"}), 0);
        QCOMPARE(read(log), commands);
        QFile fallback(dir.path() + "/fallback");
        QVERIFY(fallback.open(QIODevice::WriteOnly)); fallback.close();
        QCOMPARE(run(base + QStringList{"install", "1234:abcd:mouse"}), 0);
        const QByteArray previous = read(rules(dir));
        QFile diskFull(dir.path() + "/fail-write");
        QVERIFY(diskFull.open(QIODevice::WriteOnly)); diskFull.close();
        QCOMPARE(run(base + QStringList{"install", "--no-reload", "054c:0ce6:gamepad"}), 3);
        QCOMPARE(read(rules(dir)), previous);
        QCOMPARE(QDir(QFileInfo(rules(dir)).path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size(), 1);
    }
    void removeStillRevokesAclsWhenTheUdevReloadFails() {
        const QString fixture = qEnvironmentVariable("INPUT_SETUP_RELOAD_HELPER");
        QVERIFY2(QFileInfo::exists(fixture), "Set INPUT_SETUP_RELOAD_HELPER to the reload fixture");
        QScopedValueRollback<QString> binary(helper, fixture);
        QTemporaryDir dir;
        const QString r = dir.path();
        const QStringList base = {"--root", r};
        QCOMPARE(run(base + QStringList{"install", "--no-reload", "41e4:211a:keyboard"}), 0);
        put(r + "/sys/class/input/event10/device/id/vendor", "41e4\n");
        put(r + "/sys/class/input/event10/device/id/product", "211a\n");
        put(r + "/sys/class/input/event10/dev", "13:74\n");
        put(r + "/run/udev/data/c13:74", "I:1\nQ:seat\n");
        QFile fail(r + "/fail-reload");
        QVERIFY(fail.open(QIODevice::WriteOnly)); fail.close();
        qputenv("INPUT_SETUP_TEST_UID", "1000");
        QCOMPARE(run(base + QStringList{"remove"}), 4);                    // the reload failure is still reported...
        qunsetenv("INPUT_SETUP_TEST_UID");
        QVERIFY(!QFileInfo::exists(rules(dir)));
        QCOMPARE(read(r + "/setfacl.log").trimmed(), QByteArray("-x u:1000 ") + r.toUtf8() + "/dev/input/event10");   // ...but access has ended
    }
    void unexpectedContents() {
        QTemporaryDir dir;
        const QStringList base = {"--root", dir.path(), "--no-reload"};
        QVERIFY(QDir().mkpath(QFileInfo(rules(dir)).path()));
        QFile file(rules(dir));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# manually managed rule\n"); file.close();
        QCOMPARE(run(base + QStringList{"install", "1234:abcd:mouse"}), 3);
        QCOMPARE(run(base + QStringList{"remove"}), 3);
        QCOMPARE(read(rules(dir)), QByteArray("# manually managed rule\n"));
    }
    void unsafeTargets() {
        QTemporaryDir dir;
        const QStringList base = {"--root", dir.path(), "--no-reload"};
        QVERIFY(QDir().mkpath(QFileInfo(rules(dir)).path()));
        QFile victim(dir.path() + "/victim");
        QVERIFY(victim.open(QIODevice::WriteOnly));
        victim.write("preserve"); victim.close();
        QVERIFY(::symlink(qPrintable(victim.fileName()), qPrintable(rules(dir))) == 0);
        for (const auto &cmd : QStringList{"status", "remove", "install"}) {
            auto args = base + QStringList{cmd};
            if (cmd == "install") args << "1234:abcd:mouse";
            QCOMPARE(run(args), 3);
            QCOMPARE(read(victim.fileName()), QByteArray("preserve"));
            QVERIFY(QFileInfo(rules(dir)).isSymLink());
        }
        QVERIFY(QFile::remove(rules(dir)));
        QVERIFY(::link(qPrintable(victim.fileName()), qPrintable(rules(dir))) == 0);
        QCOMPARE(run(base + QStringList{"install", "1234:abcd:mouse"}), 3);
        QCOMPARE(run(base + QStringList{"remove"}), 3);
        QCOMPARE(read(victim.fileName()), QByteArray("preserve"));
        QVERIFY(QFile::remove(rules(dir)));
        QVERIFY(::mkfifo(qPrintable(rules(dir)), 0600) == 0);
        QCOMPARE(run(base + QStringList{"install", "1234:abcd:mouse"}), 3);
        QVERIFY(QFile::remove(rules(dir)));
        QVERIFY(QDir().mkdir(rules(dir)));
        QCOMPARE(run(base + QStringList{"remove"}), 3);
        QTemporaryDir parent;
        QVERIFY(::symlink(qPrintable(dir.path()), qPrintable(parent.path() + "/linked")) == 0);
        QCOMPARE(run({"--root", parent.path() + "/linked", "--no-reload", "status"}), 3);
        QCOMPARE(run({"--root", victim.fileName(), "--no-reload", "status"}), 3);
    }
};
QTEST_GUILESS_MAIN(HelperTests)
#include "tst_helper.moc"
