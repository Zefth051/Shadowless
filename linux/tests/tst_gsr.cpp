#include "../capture/gsr_capture.h"
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalServer>
#include <QSignalSpy>
#include <QTest>

using namespace Captail;

class GsrTests : public QObject {
    Q_OBJECT
    const GsrCapabilities supported{"6.1.5", "nvidia", "wayland", {"h264", "hevc"}, {"DP-1", "portal"}};
private slots:
    void capabilities() {
        auto info = GsrCapabilities::parse(
            "section=system_info\ndisplay_server|wayland\ngsr_version|6.1.5\n"
            "section=gpu_info\nvendor|nvidia\nsection=video_codecs\nh264\nh264_software\nhevc\nhevc_hdr\nhevc_vulkan\n"
            "section=capture_options\nDP-1|1920x1080\n/dev/video0|1920x1080@30hz|mjpeg\nregion\nportal\n");
        QCOMPARE(info.version, "6.1.5");
        QCOMPARE(info.vendor, "nvidia");
        QCOMPARE(info.displayServer, "wayland");
        QCOMPARE(info.codecs, QStringList({"h264", "hevc"}));
        QCOMPARE(info.sources, QStringList({"DP-1", "portal"}));
    }

    void invalidOptions() {
        GsrOptions options;
        options.outputDirectory = "/tmp/clips";
        QVERIFY(options.validate(supported).isEmpty());
        auto invalid = options;
        invalid.codec = "av1"; QVERIFY(!invalid.validate(supported).isEmpty());
        invalid = options; invalid.source = "HDMI-99"; QVERIFY(!invalid.validate(supported).isEmpty());
        invalid = options; invalid.outputDirectory.clear(); QVERIFY(!invalid.validate(supported).isEmpty());
        for (int fps : {0, 241}) { invalid = options; invalid.frameRate = fps; QVERIFY(!invalid.validate(supported).isEmpty()); }
        for (int seconds : {0, 3601}) { invalid = options; invalid.bufferSeconds = seconds; QVERIFY(!invalid.validate(supported).isEmpty()); }
        for (int rate : {-1, 101}) { invalid = options; invalid.bitrateMbps = rate; QVERIFY(!invalid.validate(supported).isEmpty()); }
        invalid = options; invalid.container = "exe"; QVERIFY(!invalid.validate(supported).isEmpty());
    }

    void captureArguments() {
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = "/tmp/clips with spaces/$(literal)";
        options.microphone = true;
        options.bitrateMbps = 35;
        auto args = options.arguments("/tmp/engine.sock");
        auto value = [&args](const QString &flag) { return args.value(args.indexOf(flag) + 1); };
        QCOMPARE(value("-o"), options.outputDirectory);
        QCOMPARE(value("-a"), "default_output|default_input");
        QCOMPARE(value("-fallback-cpu-encoding"), "no");
        QCOMPARE(value("-bm"), "cbr");
        QCOMPARE(value("-q"), "35000");
        QCOMPARE(value("-r"), "30");
        QCOMPARE(value("-restart-replay-on-save"), "no");
        options.separateAudioTracks = true;
        args = options.arguments("/tmp/engine.sock");
        QCOMPARE(args.count("-a"), 2);
        options.continuousRecording = true;
        args = options.arguments("/tmp/engine.sock");
        QVERIFY(!args.contains("-r"));
        QVERIFY(value("-o").startsWith(options.outputDirectory + "/Recording_"));
        QVERIFY(value("-o").endsWith(".mp4"));
    }

    void ipcReplies_data() {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<bool>("success");
        QTest::addColumn<QString>("result");
        QTest::newRow("saved path") << QByteArray("{\"id\":42,\"result\":\"ok\",\"data\":\"/tmp/clip with spaces.mp4\"}\n") << true << QString("/tmp/clip with spaces.mp4");
        QTest::newRow("engine error") << QByteArray("{\"id\":42,\"result\":\"error\",\"data\":\"no recording is running\"}\n") << false << QString("no recording is running");
        QTest::newRow("wrong id") << QByteArray("{\"id\":99,\"result\":\"ok\"}\n") << false << QString();
        QTest::newRow("malformed") << QByteArray("not json\n") << false << QString();
        QTest::newRow("unknown result") << QByteArray("{\"id\":42,\"result\":\"surprise\"}\n") << false << QString();
    }

    void ipcReplies() {
        QFETCH(QByteArray, reply); QFETCH(bool, success); QFETCH(QString, result);
        QTemporaryDir directory;
        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("ipc.sock")));
        QByteArray received;
        connect(&server, &QLocalServer::newConnection, this, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QLocalSocket::readyRead, this, [&, peer] {
                received += peer->readAll();
                if (!received.contains('\n')) return;
                peer->write(reply.left(12));
                QTimer::singleShot(15, peer, [peer, reply] { peer->write(reply.mid(12)); });
            });
        });
        GsrRequest request;
        QSignalSpy finished(&request, &GsrRequest::finished);
        request.send(server.fullServerName(), 42, "save-replay", QJsonObject{{"seconds", 7}}, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1500);
        QCOMPARE(finished[0][0].toBool(), success);
        if (!result.isEmpty()) QCOMPARE(finished[0][1].toString(), result);
        auto message = QJsonDocument::fromJson(received).object();
        QCOMPARE(message["name"].toString(), "save-replay");
        QCOMPARE(message["data"].toObject()["seconds"].toInt(), 7);
    }

    void ipcTimeout() {
        QTemporaryDir directory;
        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("ipc.sock")));
        GsrRequest request;
        QSignalSpy finished(&request, &GsrRequest::finished);
        request.send(server.fullServerName(), 1, "save-replay", {}, 30);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished[0][0].toBool(), false);
        QVERIFY(finished[0][1].toString().contains("timed out"));
    }

    void failedLaunch() {
        GsrCapture engine(nullptr, "/no/such/captail-gsr-engine");
        GsrOptions options; options.outputDirectory = QDir::tempPath();
        QSignalSpy failed(&engine, &GsrCapture::error);
        engine.start(options, supported);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 1500);
        QVERIFY(failed[0][0].toString().contains("Cannot start"));
        QVERIFY(!engine.active());
        QVERIFY(!engine.running());
    }

    void liveCapture_data() {
        QTest::addColumn<QString>("codec");
        QTest::newRow("h264") << QString("h264");
        QTest::newRow("hevc") << QString("hevc");
    }

    void liveCapture() {
        if (qEnvironmentVariable("CAPTAIL_GSR_LIVE") != "1")
            QSKIP("Set CAPTAIL_GSR_LIVE=1 to record the selected monitor and system audio.");
        QFETCH(QString, codec);
        QString queryError;
        auto capabilities = GsrCapabilities::probe(&queryError);
        QVERIFY2(queryError.isEmpty(), qPrintable(queryError));
        if (!capabilities.codecs.contains(codec)) QSKIP("Hardware codec is unavailable on this GPU.");
        const auto root = qEnvironmentVariable("CAPTAIL_QA_OUTPUT", QDir::currentPath() + "/live-clips");
        const auto output = QDir(root).filePath(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz-") + codec);
        QVERIFY(QDir().mkpath(output));
        QFile log(QDir(output).filePath("capture.log"));
        QVERIFY(log.open(QIODevice::WriteOnly));
        GsrCapture engine;
        connect(&engine, &GsrCapture::diagnostic, &log, [&log](const QString &message) { log.write(message.toUtf8() + '\n'); log.flush(); });
        QSignalSpy started(&engine, &GsrCapture::started), stopped(&engine, &GsrCapture::stopped);
        QSignalSpy clips(&engine, &GsrCapture::clipSaved), errors(&engine, &GsrCapture::error);
        QSignalSpy recording(&engine, &GsrCapture::recordingChanged);
        GsrOptions options;
        options.source = qEnvironmentVariable("CAPTAIL_GSR_SOURCE", "DP-1");
        options.outputDirectory = output;
        options.codec = codec;
        options.bufferSeconds = 10;
        engine.start(options, capabilities);
        QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty() || !errors.isEmpty(), 15000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        QVERIFY(engine.running());
        QTest::qWait(3500);
        engine.saveReplay(2);
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 1, 10000);
        engine.startReplayRecording();
        QTRY_VERIFY_WITH_TIMEOUT(!recording.isEmpty() && recording.last()[0].toBool(), 5000);
        QTest::qWait(3000);
        engine.stopReplayRecording();
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 2, 10000);
        engine.saveReplay(2);
        // Stop must wait for this save instead of killing a pending snapshot.
        engine.stop();
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 3, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 1, 10000);
        QVERIFY(!engine.active());
        QVERIFY(!engine.running());

        options.continuousRecording = true;
        engine.start(options, capabilities);
        QTRY_COMPARE_WITH_TIMEOUT(started.size(), 2, 15000);
        QTest::qWait(3000);
        engine.stop();
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 4, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 2, 10000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        for (const auto &saved : clips) {
            const auto path = saved[0].toString();
            QProcess probe;
            probe.start("ffprobe", {"-v", "error", "-show_entries", "stream=codec_name,width,height,avg_frame_rate:format=duration", "-of", "json", path});
            QVERIFY(probe.waitForFinished(10000));
            QCOMPARE(probe.exitCode(), 0);
            const auto metadata = probe.readAllStandardOutput();
            QFile report(path + ".json"); QVERIFY(report.open(QIODevice::WriteOnly)); report.write(metadata);
            const auto info = QJsonDocument::fromJson(metadata).object();
            const auto streams = info["streams"].toArray();
            QVERIFY(streams.size() >= 2);
            QCOMPARE(streams[0].toObject()["codec_name"].toString(), codec);
            QVERIFY(streams[0].toObject()["width"].toInt() > 0);
            QVERIFY(streams[0].toObject()["height"].toInt() > 0);
            QCOMPARE(streams[1].toObject()["codec_name"].toString(), "aac");
            QVERIFY(info["format"].toObject()["duration"].toString().toDouble() > 1.0);
            QProcess decode;
            decode.start("ffmpeg", {"-v", "error", "-i", path, "-f", "null", "-"});
            QVERIFY(decode.waitForFinished(30000));
            QCOMPARE(decode.exitCode(), 0);
            QVERIFY2(decode.readAllStandardError().isEmpty(), qPrintable(path));
        }
        qInfo().noquote() << "Validated clips:" << output;
    }
};

QTEST_GUILESS_MAIN(GsrTests)
#include "tst_gsr.moc"
