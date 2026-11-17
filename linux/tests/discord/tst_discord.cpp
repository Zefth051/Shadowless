#include "../../app/discord_presence.h"
#include "../../capture/replay_paths.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>
#include <sys/socket.h>
#include <unistd.h>

namespace Shadowless {
class DiscordPresenceTests : public QObject {
    Q_OBJECT
    struct Session {
        DiscordPresence &presence;
        QLocalSocket peer;
        explicit Session(DiscordPresence &client) : presence(client) {
            int sockets[2];
            if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) qFatal("socketpair failed");
            if (!presence.m_socket.setSocketDescriptor(sockets[0]) || !peer.setSocketDescriptor(sockets[1]))
                qFatal("socket descriptor failed");
        }
        QByteArray output() {
            presence.m_socket.flush();
            if (!peer.bytesAvailable()) peer.waitForReadyRead(100);
            return peer.readAll();
        }
        void sendBytes(const QByteArray &bytes) {
            peer.write(bytes); peer.flush();
            presence.m_socket.waitForReadyRead(100);
        }
        void send(const QJsonObject &message) {
            sendBytes(DiscordPresence::frame(1, QJsonDocument(message).toJson(QJsonDocument::Compact)));
        }
        void ready() { send({{"cmd", "DISPATCH"}, {"evt", "READY"}}); }
    };
    static QJsonObject payload(const QByteArray &bytes) { return QJsonDocument::fromJson(bytes.mid(8)).object(); }
private slots:
    void ambiguousProcessIdentity_data() {
        QTest::addColumn<QString>("executable");
        QTest::addColumn<QByteArray>("arguments");
        QTest::addColumn<QString>("expected");
        QTest::newRow("known native") << "rocketleague.exe" << QByteArray() << QString("Rocket League");
        QTest::newRow("unknown native") << "unknown-game" << QByteArray() << QString();
        QTest::newRow("known wine") << "wine64"
            << (QByteArray("C:\\Games\\rocketleague.exe") + '\0') << QString("Rocket League");
        QTest::newRow("ambiguous before known") << "wine64"
            << (QByteArray("game.exe") + '\0' + "rocketleague.exe" + '\0') << QString();
        QTest::newRow("ambiguous after known") << "wine64"
            << (QByteArray("rocketleague.exe") + '\0' + "game.exe" + '\0') << QString();
        QTest::newRow("ambiguous process alongside known") << "game.exe" << QByteArray() << QString();
    }
    void ambiguousProcessIdentity() {
        QFETCH(QString, executable);
        QFETCH(QByteArray, arguments);
        QFETCH(QString, expected);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.filePath("proc");
        QVERIFY(QDir().mkpath(root + "/101"));
        QVERIFY(QDir().mkpath(root + "/102"));
        QFile binary(directory.filePath(executable));
        QVERIFY(binary.open(QIODevice::WriteOnly)); binary.close();
        QVERIFY(QFile::link(binary.fileName(), root + "/101/exe"));
        QFile cmdline(root + "/101/cmdline");
        QVERIFY(cmdline.open(QIODevice::WriteOnly));
        QCOMPARE(cmdline.write(arguments), arguments.size()); cmdline.close();
        // A second known process must not override an ambiguous identity.
        if (executable == "game.exe") {
            QFile known(directory.filePath("rocketleague.exe"));
            QVERIFY(known.open(QIODevice::WriteOnly)); known.close();
            QVERIFY(QFile::link(known.fileName(), root + "/102/exe"));
        }
        QCOMPARE(gameForProcesses(root), expected);
        if (expected.isEmpty()) {
            const auto clip = directory.filePath("clip.mp4");
            QFile file(clip); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("original clip"); file.close();
            QString error;
            QCOMPARE(routeClip(directory.path(), clip, gameForProcesses(root), &error), clip);
            QVERIFY(error.isEmpty());
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), QByteArray("original clip"));
        }
    }
    void littleEndianFrames() {
        QCOMPARE(DiscordPresence::frame(3, "ping"), QByteArray::fromHex("030000000400000070696e67"));
        QVERIFY(DiscordPresence::frame(1, QByteArray(65537, 'x')).isEmpty());
    }
    void waitsForReadyAndCoalescesStates() {
        DiscordPresence presence; Session session(presence);
        presence.setState(DiscordPresence::Recording);
        QVERIFY(session.output().isEmpty());
        presence.handshake();
        const auto hello = session.output();
        QCOMPARE(qFromLittleEndian<quint32>(hello.constData()), 0U);
        QCOMPARE(payload(hello)["v"].toInt(), 1);
        QCOMPARE(payload(hello)["client_id"].toString(), "1571906955054948393");
        presence.setState(DiscordPresence::InstantClip);
        presence.setState(DiscordPresence::Paused);
        const auto ready = DiscordPresence::frame(1, R"({"cmd":"DISPATCH","evt":"READY"})");
        session.sendBytes(ready.first(3)); QVERIFY(session.output().isEmpty());
        session.sendBytes(ready.mid(3));
        const auto command = payload(session.output());
        QCOMPARE(command["cmd"].toString(), "SET_ACTIVITY");
        const auto args = command["args"].toObject();
        QCOMPARE(args["pid"].toInteger(), QCoreApplication::applicationPid());
        QCOMPARE(args["activity"].toObject().size(), 2);
        QVERIFY(!args["activity"].toObject().contains("state"));
        QCOMPARE(args["activity"].toObject()["details"].toString(), "Recording paused");
        presence.setState(DiscordPresence::Idle);
        presence.setState(DiscordPresence::Recording);
        QVERIFY(session.output().isEmpty());
        session.send({{"cmd", "SET_ACTIVITY"}, {"nonce", "wrong"}});
        QVERIFY(!presence.m_nonce.isEmpty());
        session.send({{"cmd", "SET_ACTIVITY"}, {"nonce", command["nonce"]}});
        QVERIFY(presence.m_nonce.isEmpty()); QVERIFY(session.output().isEmpty());
        presence.m_update.stop(); presence.publish();
        const auto latest = payload(session.output());
        QCOMPARE(latest["args"].toObject()["activity"].toObject()["details"].toString(), "Recording");
        session.send({{"cmd", "SET_ACTIVITY"}, {"nonce", latest["nonce"]}});
        presence.m_update.stop(); presence.publish(); QVERIFY(session.output().isEmpty());
    }
    void showsIconsWithoutClockOrPeriodicRefresh() {
        DiscordPresence presence; Session session(presence);
        presence.m_gameIcons = DiscordPresence::gameIconsFromJson(R"([
            {"name":"Rocket League","id":"123456789012345678","icon":"0123456789abcdef0123456789abcdef"},
            {"name":"Invalid","id":"../bad","icon":"0123456789abcdef0123456789abcdef"},
            {"name":"No icon","id":"123456789012345679","icon":null}
        ])");
        QVERIFY(!presence.m_gameIcons.contains("Invalid"));
        QVERIFY(!presence.m_gameIcons.contains("No icon"));
        QVERIFY(DiscordPresence::gameIconsFromJson("bad JSON").isEmpty());
        presence.setState(DiscordPresence::InstantClip, "Rocket League");
        session.ready();
        auto command = payload(session.output());
        const auto activity = command["args"].toObject()["activity"].toObject();
        QCOMPARE(activity["details"].toString(), "Clipping \"Rocket League\"");
        QVERIFY(!activity.contains("state")); QVERIFY(!activity.contains("timestamps"));
        const auto assets = activity["assets"].toObject();
        QVERIFY(assets["large_image"].toString().endsWith("/linux/app/assets/shadowless-moon.png"));
        QCOMPARE(assets["large_text"].toString(), "Shadowless");
        QCOMPARE(assets["small_image"].toString(), "https://cdn.discordapp.com/app-icons/123456789012345678/0123456789abcdef0123456789abcdef.png");
        QCOMPARE(assets["small_text"].toString(), "Rocket League");
        session.send({{"cmd", "SET_ACTIVITY"}, {"nonce", command["nonce"]}});
        presence.m_update.setInterval(1); presence.m_update.start();
        QTRY_VERIFY(!presence.m_update.isActive()); QVERIFY(session.output().isEmpty());
        presence.setState(DiscordPresence::Idle, "Rocket League");
        QVERIFY(payload(session.output())["args"].toObject()["activity"].isNull());
        QVERIFY(presence.m_game.isEmpty());
    }
    void handlesPingAndClearsOnStop() {
        DiscordPresence presence; Session session(presence);
        session.ready(); session.output();
        const QByteArray ping("p\0ng", 4);
        session.sendBytes(DiscordPresence::frame(3, ping));
        QCOMPARE(session.output(), DiscordPresence::frame(4, ping));
        presence.stop();
        const auto clear = payload(session.output());
        QCOMPARE(clear["cmd"].toString(), "SET_ACTIVITY");
        QVERIFY(clear["args"].toObject()["activity"].isNull());
        QVERIFY(!presence.m_retry.isActive());
    }
    void rejectsErrorsAndOversizedFrames_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("error response") << DiscordPresence::frame(1, R"({"evt":"ERROR","data":{"code":4000}})");
        QTest::newRow("close") << DiscordPresence::frame(2, "{}");
        QTest::newRow("malformed JSON") << DiscordPresence::frame(1, "bad");
        QTest::newRow("oversized") << QByteArray::fromHex("0100000001000100");
        QTest::newRow("unknown opcode") << DiscordPresence::frame(42, "{}");
    }
    void rejectsErrorsAndOversizedFrames() {
        QFETCH(QByteArray, bytes);
        DiscordPresence presence; Session session(presence);
        session.ready(); session.output();
        session.sendBytes(bytes);
        QVERIFY(!presence.m_ready);
        QCOMPARE(presence.m_socket.state(), QLocalSocket::UnconnectedState);
        QVERIFY(presence.m_input.isEmpty()); QVERIFY(presence.m_nonce.isEmpty());
    }
    void incompleteFrameTimesOutQuietly() {
        DiscordPresence presence; Session session(presence);
        presence.handshake(); session.output();
        session.sendBytes(DiscordPresence::frame(1, "{}").first(4));
        QVERIFY(presence.m_deadline.isActive());
        presence.m_deadline.setInterval(1);
        QTRY_COMPARE(presence.m_socket.state(), QLocalSocket::UnconnectedState);
        QVERIFY(!presence.m_ready);
    }
    void multipleFramesStaySeparate() {
        DiscordPresence presence; Session session(presence);
        session.sendBytes(DiscordPresence::frame(3, "one") + DiscordPresence::frame(3, "two"));
        QCOMPARE(session.output(), DiscordPresence::frame(4, "one") + DiscordPresence::frame(4, "two"));
        QVERIFY(presence.m_input.isEmpty());
        QCOMPARE(DiscordPresence::stateText(DiscordPresence::Idle), "Idle");
        QCOMPARE(DiscordPresence::stateText(DiscordPresence::InstantClip), "Instant Clip enabled");
    }
    void reconnectRetainsLatestState() {
        DiscordPresence presence;
        {
            Session session(presence); session.ready(); session.output();
            presence.setState(DiscordPresence::Unknown);
            session.sendBytes(DiscordPresence::frame(2, "{}"));
        }
        Session restarted(presence);
        presence.handshake(); restarted.output();
        restarted.ready();
        QCOMPARE(payload(restarted.output())["args"].toObject()["activity"].toObject()["details"].toString(),
                 "Recording state unknown");
    }
};
}
QTEST_GUILESS_MAIN(Shadowless::DiscordPresenceTests)
#include "tst_discord.moc"
