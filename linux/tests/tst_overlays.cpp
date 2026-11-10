#include "../overlays/layer.h"
#include "../overlays/media.h"
#include "../settings/settings.h"
#include "../plugins/compositor.h"
#include "../overlays/overlay_client.h"
#include "../platform/hyprland/bindings.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QImage>
#include <limits>

using namespace Shadowless;

class OverlayTests : public QObject {
    Q_OBJECT
private slots:
    void bindingsPreserveOtherApplications() {
        HyprBinding binding; QVERIFY(HyprBinding::parse("Alt+Z", "toggle-overlay", &binding));
        QCOMPARE(binding.mask, 8); QCOMPARE(binding.key, "Z");
        QCOMPARE(binding.availability({}), HyprBinding::Free);
        QJsonObject other{{"modmask", 8}, {"key", "Z"}, {"dispatcher", "exec"}, {"arg", "another-application"}};
        QCOMPARE(binding.availability(QJsonArray{other}), HyprBinding::Conflict);
        other["dispatcher"] = "global"; other["arg"] = "io.github.zefth051.Shadowless:toggle-overlay";
        QCOMPARE(binding.availability(QJsonArray{other}), HyprBinding::Existing);
        other["modmask"] = 64; QCOMPARE(binding.availability(QJsonArray{other}), HyprBinding::Free);
        QVERIFY(!HyprBinding::parse("Alt+Z, Alt+Y", "toggle-overlay", &binding));
    }
    void geometryStaysInsideCanvas() {
        OverlayLayer layer;
        layer.width = .4; layer.height = .3;
        layer.setPosition(1., -5.);
        QCOMPARE(layer.x, .6); QCOMPARE(layer.y, 0.);
        layer.width = 2.; layer.height = std::numeric_limits<double>::quiet_NaN();
        layer.normalize();
        QCOMPARE(layer.width, 1.); QCOMPARE(layer.height, .25); QCOMPARE(layer.x, 0.);
        QCOMPARE(layer.fitRect(QSize(1000, 500), QSize(100, 100)), QRectF(437.5, 0, 125, 125));
    }

    void sceneRoundTripPreservesOrder() {
        OverlayScene scene;
        scene.enabled = true;
        OverlayLayer top; top.path = "/tmp/top.png"; top.opacity = 37;
        OverlayLayer bottom; bottom.path = "/tmp/bottom.mp4";
        scene.layers = {top, bottom};
        OverlayScene restored; QString error;
        QVERIFY2(OverlayScene::fromJson(scene.toJson(), &restored, &error), qPrintable(error));
        QCOMPARE(restored.layers.size(), 2);
        QCOMPARE(restored.layers[0].path, "/tmp/top.png");
        QCOMPARE(restored.layers[0].opacity, 37);
        QCOMPARE(restored.layers[1].path, "/tmp/bottom.mp4");
    }

    void rejectsUnboundedAndMalformedScenes() {
        OverlayScene scene; QString error;
        QVERIFY(!OverlayScene::fromJson(QJsonObject{}, &scene, &error));
        QJsonObject value{{"version", 1}, {"enabled", true}, {"layers", QJsonArray{}}};
        QVERIFY(OverlayScene::fromJson(value, &scene, &error));
        OverlayLayer layer; layer.path = "/tmp/a.png";
        auto row = layer.toJson();
        row["x"] = "not a number";
        value["layers"] = QJsonArray{row};
        QVERIFY(!OverlayScene::fromJson(value, &scene, &error));
        row = layer.toJson(); row["path"] = "https://example.com/a.png";
        value["layers"] = QJsonArray{row};
        QVERIFY(!OverlayScene::fromJson(value, &scene, &error));
        QJsonArray many;
        for (int i = 0; i < 17; ++i) { row = layer.toJson(); row["id"] = QString::number(i); many.append(row); }
        value["layers"] = many;
        QVERIFY(!OverlayScene::fromJson(value, &scene, &error));
    }

    void staticMediaDecodesOnce() {
        QTemporaryDir directory;
        const auto path = directory.filePath("red image.png");
        QImage image(40, 20, QImage::Format_RGBA8888); image.fill(Qt::red);
        QVERIFY(image.save(path));
        Media media; QString error;
        QVERIFY2(media.open(path, QSize(20, 20), &error), qPrintable(error));
        QCOMPARE(media.frame().size(), QSize(20, 10));
        QCOMPARE(media.frame().pixelColor(3, 3), QColor(Qt::red));
        QVERIFY(!media.advance(1000));
        QVERIFY(!media.advance(100000));
        QVERIFY(!media.open(directory.filePath("missing.png"), QSize(20, 20), &error));
        QVERIFY(!error.isEmpty());
    }

    void animatedWebpAdvancesAndLoops() {
        QTemporaryDir directory;
        QImage first(40, 20, QImage::Format_RGBA8888); first.fill(Qt::red);
        QImage second(40, 20, QImage::Format_RGBA8888); second.fill(Qt::blue);
        QVERIFY(first.save(directory.filePath("frame0.png"))); QVERIFY(second.save(directory.filePath("frame1.png")));
        QProcess encoder;
        encoder.start("ffmpeg", {"-v", "error", "-y", "-framerate", "2", "-i", directory.filePath("frame%d.png"), "-c:v", "libwebp_anim", "-lossless", "1", "-loop", "0", directory.filePath("animation.webp")});
        QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
        Media media; QString error;
        QVERIFY2(media.open(directory.filePath("animation.webp"), QSize(40, 20), &error), qPrintable(error));
        QVERIFY(media.animated()); QCOMPARE(media.frame().pixelColor(10, 10), QColor(Qt::red));
        QVERIFY(media.advance(600)); QCOMPARE(media.frame().pixelColor(10, 10), QColor(Qt::blue));
        QVERIFY(media.advance(1200)); QCOMPARE(media.frame().pixelColor(10, 10), QColor(Qt::red));
    }

    void settingsRetainCaptureAndLayers() {
        QTemporaryDir directory;
        Settings settings(directory.filePath("settings.ini"));
        GsrOptions options; options.source = "DP-1"; options.codec = "hevc";
        options.outputDirectory = "/tmp/clips"; options.bitrateMbps = 35; options.microphone = true;
        OverlayScene scene; scene.enabled = true;
        OverlayLayer layer; layer.path = "/tmp/a.png"; scene.layers.append(layer);
        QString error;
        QVERIFY2(settings.save(options, scene, true, &error), qPrintable(error));
        Settings loaded(directory.filePath("settings.ini"));
        const GsrCapabilities caps{"6.1.5", "nvidia", "wayland", {"h264", "hevc"}, {"DP-1"}};
        QCOMPARE(loaded.capture(caps).codec, "hevc");
        QCOMPARE(loaded.capture(caps).bitrateMbps, 35);
        QVERIFY(loaded.capture(caps).microphone);
        QCOMPARE(loaded.scene().layers.size(), 1);
        QVERIFY(loaded.indicator());
    }

    void workerAppliesAtomicallyAndClearsDisabledLayers() {
        QTemporaryDir directory; QString error;
        QImage image(40, 20, QImage::Format_RGBA8888); image.fill(Qt::green);
        const auto path = directory.filePath("green.png"); QVERIFY(image.save(path));
        CompositorWorker worker;
        const auto socket = directory.filePath("overlay.sock");
        QVERIFY2(worker.start(socket, QSize(1920, 1080), &error), qPrintable(error));
        OverlayScene scene; scene.enabled = true;
        OverlayLayer layer; layer.path = path; scene.layers.append(layer);
        OverlayClient client; QSignalSpy finished(&client, &OverlayClient::finished);
        client.apply(socket, scene);
        QTRY_COMPARE(finished.size(), 1); QVERIFY(finished.takeFirst()[0].toBool());
        QVector<RenderLayer> frames; QVERIFY(worker.snapshot(&frames));
        QCOMPARE(frames.size(), 1); QCOMPARE(frames[0].image.pixelColor(0, 0), QColor(Qt::green));
        scene.layers[0].path = directory.filePath("missing.png"); client.apply(socket, scene);
        QTRY_COMPARE(finished.size(), 1); QVERIFY(!finished.takeFirst()[0].toBool());
        QVERIFY(worker.snapshot(&frames)); QCOMPARE(frames.size(), 1);
        scene.enabled = false; client.apply(socket, scene);
        QTRY_COMPARE(finished.size(), 1); QVERIFY(finished.takeFirst()[0].toBool());
        QVERIFY(worker.snapshot(&frames)); QVERIFY(frames.isEmpty());
    }

    void workerHandlesFragmentedAndMalformedMessages() {
        QTemporaryDir directory; QString error; CompositorWorker worker;
        const auto path = directory.filePath("socket"); QVERIFY(worker.start(path, QSize(100, 100), &error));
        QLocalSocket socket; socket.connectToServer(path); QVERIFY(socket.waitForConnected(1000));
        socket.write("{\"version\":1,"); socket.flush(); QTest::qWait(30);
        QCOMPARE(socket.bytesAvailable(), 0);
        socket.write("\"enabled\":false,\"layers\":[]}\n"); socket.flush();
        QTRY_VERIFY(socket.bytesAvailable() > 0);
        QVERIFY(QJsonDocument::fromJson(socket.readAll()).object().value("ok").toBool());
        socket.abort(); socket.connectToServer(path); QVERIFY(socket.waitForConnected(1000));
        socket.write("not json\n"); socket.flush(); QTRY_VERIFY(socket.bytesAvailable() > 0);
        QVERIFY(!QJsonDocument::fromJson(socket.readAll()).object().value("ok").toBool());
        socket.abort(); socket.connectToServer(path); QVERIFY(socket.waitForConnected(1000));
        socket.write(QByteArray(70000, 'x')); socket.flush();
        QTRY_COMPARE(socket.state(), QLocalSocket::UnconnectedState);
    }
};

QTEST_GUILESS_MAIN(OverlayTests)
#include "tst_overlays.moc"
