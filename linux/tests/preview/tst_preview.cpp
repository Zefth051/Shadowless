#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QPushButton>
#include "../../overlays/preview_client.h"
#include "../../plugins/compositor.h"
#include "../../ui/overlay_editor.h"
using namespace Shadowless;
class PreviewTests : public QObject {
    Q_OBJECT
private slots:
    void editorSwitchesBetweenRecordedOutputAndLayers() {
        QTemporaryDir dir;
        CompositorWorker worker; QString error;
        const auto socket = dir.filePath("scene");
        QVERIFY(worker.start(socket, QSize(1600, 900), &error));
        QImage asset(64, 64, QImage::Format_RGBA8888); asset.fill(Qt::red);
        const auto path = dir.filePath("layer.png"); QVERIFY(asset.save(path));
        OverlayLayer layer; layer.path = path; layer.x = .25; layer.y = .25;
        layer.width = .5; layer.height = .5;
        OverlayScene scene; scene.enabled = true; scene.layers.append(layer);
        OverlayEditor editor; editor.resize(960, 640); editor.edit(scene);
        editor.setPreviewSocket(socket + ".preview"); editor.show();
        auto *live = editor.findChild<QPushButton *>("livePreview");
        auto *layers = editor.findChild<QPushButton *>("layersPreview");
        QVERIFY(live); QVERIFY(layers); QVERIFY(live->isChecked());
        auto *canvas = editor.findChild<OverlayCanvas *>(); QVERIFY(canvas);
        QImage capture(160, 90, QImage::Format_RGBA8888); capture.fill(Qt::green);
        QTimer feed;
        connect(&feed, &QTimer::timeout, this, [&] { if (worker.previewRequested()) worker.publishPreview(capture); });
        feed.start(10);
        auto pixels = [canvas](QColor color) {
            const auto image = canvas->grab().toImage(); int count = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    count += image.pixelColor(x, y) == color;
            return count;
        };
        QTRY_VERIFY(pixels(Qt::green) > 100);
        QVERIFY(pixels(Qt::red) > 100);
        scene.enabled = false; editor.edit(scene);
        QTRY_COMPARE(pixels(Qt::red), 0);
        layers->click();
        QTRY_COMPARE(pixels(Qt::green), 0);
        QVERIFY(pixels(Qt::red) > 100);
        QTRY_VERIFY(!worker.previewRequested());
        QCOMPARE(editor.scene().toJson(), scene.toJson());
        live->click();
        QTRY_VERIFY(pixels(Qt::green) > 100);
        QCOMPARE(pixels(Qt::red), 0);
        editor.hide(); QTRY_VERIFY(!worker.previewRequested());
    }
    void visibleOnlyAndAspect() {
        QTemporaryDir dir; QLocalServer server; QVERIFY(server.listen(dir.filePath("preview")));
        PreviewClient client; QSignalSpy frames(&client, &PreviewClient::frameReady);
        client.setSocketPath(server.fullServerName());
        QTest::qWait(300); QVERIFY(!server.hasPendingConnections());
        client.setActive(true); QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0); QCOMPARE(peer->readAll(), QByteArray("FRAME\n"));
        QImage image(4, 2, QImage::Format_RGBA8888); image.fill(Qt::red);
        image.setPixelColor(0, 0, Qt::blue);
        peer->write("SLP1 4 2 1920 960\n");
        peer->write(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()); peer->flush();
        QTRY_COMPARE(frames.size(), 1);
        QCOMPARE(qvariant_cast<QSize>(frames[0][1]), QSize(1920, 960));
        QCOMPARE(qvariant_cast<QImage>(frames[0][0]).pixelColor(0, 0), QColor(Qt::blue));
        client.setActive(false); QTest::qWait(350); QVERIFY(!server.hasPendingConnections());
    }
    void emptyDisabledSceneAndLifecycle() {
        QTemporaryDir dir; const auto path = dir.filePath("scene");
        CompositorWorker worker; QString error; QVERIFY(worker.start(path, QSize(1920, 1200), &error));
        PreviewClient client; client.setSocketPath(path + ".preview");
        QSignalSpy frames(&client, &PreviewClient::frameReady); client.setActive(true);
        QTRY_VERIFY(worker.previewRequested());
        QImage image(640, 400, QImage::Format_RGBA8888); image.fill(Qt::green);
        QVERIFY(worker.publishPreview(image)); QTRY_COMPARE(frames.size(), 1);
        QCOMPARE(qvariant_cast<QSize>(frames[0][1]), QSize(1920, 1200));
        client.setActive(false); QTRY_VERIFY(!worker.previewRequested());
        OverlayEditor editor; editor.setPreviewSocket(path + ".preview"); editor.show();
        QTRY_VERIFY(worker.previewRequested()); editor.hide(); QTRY_VERIFY(!worker.previewRequested());
        editor.setPreviewSocket({});
    }
    void rejectsOversizedFrame() {
        QTemporaryDir dir; QLocalServer server; QVERIFY(server.listen(dir.filePath("bad")));
        PreviewClient client; QSignalSpy frames(&client, &PreviewClient::frameReady);
        QSignalSpy states(&client, &PreviewClient::statusChanged);
        client.setSocketPath(server.fullServerName()); client.setActive(true);
        QTRY_VERIFY(server.hasPendingConnections()); auto *peer = server.nextPendingConnection();
        peer->write("SLP1 999999 999999 1920 1080\n"); peer->flush();
        QTRY_VERIFY(!states.isEmpty()); QTest::qWait(100); QCOMPARE(frames.size(), 0);
        client.setActive(false);
    }
    void slowPreviewReaderDoesNotBlockScene() {
        QTemporaryDir dir; const auto path = dir.filePath("scene");
        CompositorWorker worker; QString error; QVERIFY(worker.start(path, QSize(1920, 1080), &error));
        QLocalSocket preview; preview.setReadBufferSize(1); preview.connectToServer(path + ".preview");
        QVERIFY(preview.waitForConnected()); preview.write("FRAME\n"); QVERIFY(preview.waitForBytesWritten());
        QTRY_VERIFY(worker.previewRequested());
        QImage image(640, 360, QImage::Format_RGBA8888); image.fill(Qt::red); QVERIFY(worker.publishPreview(image));
        QLocalSocket scene; scene.connectToServer(path); QVERIFY(scene.waitForConnected());
        scene.write("{\"version\":1,\"enabled\":false,\"layers\":[]}\n"); QVERIFY(scene.waitForBytesWritten());
        QVERIFY(scene.waitForReadyRead(1000));
        QVERIFY(QJsonDocument::fromJson(scene.readAll()).object().value("ok").toBool());
        preview.abort(); QTRY_VERIFY(!worker.previewRequested());
    }
    void canvasUsesSourceAspectAndShowsCaptureOff() {
        QTemporaryDir dir; QLocalServer server; QVERIFY(server.listen(dir.filePath("canvas")));
        OverlayCanvas canvas; canvas.resize(400, 400); canvas.setPreviewSocket(server.fullServerName()); canvas.show();
        QTRY_VERIFY(server.hasPendingConnections()); auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        QImage image(2, 4, QImage::Format_RGBA8888); image.fill(Qt::green);
        peer->write("SLP1 2 4 1000 2000\n"); peer->write(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()); peer->flush();
        QTRY_COMPARE(canvas.grab().toImage().pixelColor(200, 30), QColor(Qt::green));
        QVERIFY(canvas.grab().toImage().pixelColor(20, 200) != QColor(Qt::green));
        canvas.setPreviewSocket({});
        QTRY_VERIFY(canvas.grab().toImage().pixelColor(200, 30) != QColor(Qt::green));
        canvas.hide();
    }
};
QTEST_MAIN(PreviewTests)
#include "tst_preview.moc"
