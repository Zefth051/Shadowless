#include "../../player/mpv_player.h"
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QOpenGLWidget>
using namespace Shadowless;
class PlaybackTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    QString m_clip;
    static bool hasRedFrame(MpvPlayer &player) {
        const auto *surface = player.findChild<QOpenGLWidget *>();
        const auto image = surface ? const_cast<QOpenGLWidget *>(surface)->grabFramebuffer() : player.grab().toImage();
        if (image.isNull()) return false;
        const auto color = image.pixelColor(image.width() / 2, image.height() / 2);
        return color.red() > 180 && color.green() < 80 && color.blue() < 80;
    }
private slots:
    void initTestCase() {
        m_clip = m_directory.filePath("two-tracks.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-f", "lavfi", "-i", "sine=frequency=440", "-f", "lavfi", "-i", "sine=frequency=880", "-t", "4", "-map", "0:v", "-map", "1:a", "-map", "2:a", "-c:v", "libx264", "-threads", "1", "-c:a", "aac", m_clip}), 0);
    }
    void decodesSeeksMixesAndReleasesClip() {
        MpvPlayer player; player.resize(480, 270); player.show();
        QVERIFY(player.source().isEmpty());
        QSignalSpy errors(&player, &MpvPlayer::error), frames(&player, &MpvPlayer::frameRendered);
        connect(&player, &MpvPlayer::error, this, [](const QString &message) { qWarning().noquote() << message; });
        if (qEnvironmentVariableIsSet("SHADOWLESS_PLAYER_TRACE")) connect(&player, &MpvPlayer::stateChanged, this, [&] { qInfo() << player.loaded() << player.paused() << player.position() << player.duration(); });
        player.open(m_clip);
        QTRY_VERIFY_WITH_TIMEOUT(player.loaded(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() > 0, 5000);
        QTRY_COMPARE(player.audioTracks().size(), 2);
        QTRY_VERIFY(player.duration() >= 3900);
        player.pause(true); QTRY_VERIFY(player.paused());
        player.seek(1500); QTRY_VERIFY(qAbs(player.position() - 1500) < 200);
        const auto tracks = player.audioTracks();
        player.setAudioTracks({tracks[0].id, tracks[1].id});
        player.setAudioTracks({tracks[1].id});
        player.setAudioTracks({});
        player.setSpeed(1.5); player.setMuted(true); player.setVolume(25);
        player.pause(false); QTRY_VERIFY(!player.paused());
        QTRY_VERIFY(player.position() > 1700);
        QCOMPARE(errors.size(), 0);
        player.stop(); QVERIFY(player.source().isEmpty()); QVERIFY(!player.loaded());
        QTest::qWait(300); QCOMPARE(player.duration(), 0); QCOMPARE(player.position(), 0);
        QCOMPARE(errors.size(), 0);
        const int count = frames.size(); QTest::qWait(200); QCOMPARE(frames.size(), count);
    }
    void repeatedLoadsAndMissingClip() {
        MpvPlayer player; player.show(); QSignalSpy errors(&player, &MpvPlayer::error);
        for (int i = 0; i < 3; ++i) {
            player.open(m_clip); QTRY_VERIFY(player.loaded());
            QTRY_COMPARE(player.audioTracks().size(), 2);
            player.stop();
        }
        QCOMPARE(errors.size(), 0);
        player.open(m_directory.filePath("missing.mp4"));
        QTRY_VERIFY(errors.size() > 0);
        player.stop();
    }
    void pausedSeekDoesNotReachEnd() {
        MpvPlayer player; player.resize(480, 270); player.show(); player.open(m_clip);
        QTRY_VERIFY(player.loaded()); QTRY_VERIFY(player.duration() >= 3900);
        player.pause(true); QTRY_VERIFY(player.paused());
        player.seek(1500); QTRY_VERIFY(qAbs(player.position() - 1500) < 200);
        QTest::qWait(300); QVERIFY2(qAbs(player.position() - 1500) < 200, qPrintable(QString::number(player.position())));
        player.pause(false); QTRY_VERIFY(!player.paused());
        QTRY_VERIFY(player.position() > 1700);
        player.stop();
    }
    void changingAudioPreservesPausedPosition() {
        MpvPlayer player; player.resize(480, 270); player.show(); player.open(m_clip);
        QTRY_VERIFY(player.loaded()); QTRY_COMPARE(player.audioTracks().size(), 2);
        player.pause(true); QTRY_VERIFY(player.paused()); player.seek(1500);
        QTRY_VERIFY(qAbs(player.position() - 1500) < 200); QTest::qWait(200);
        const auto tracks = player.audioTracks();
        player.setAudioTracks({tracks[0].id, tracks[1].id});
        QTest::qWait(300);
        QTRY_VERIFY(hasRedFrame(player));
        QVERIFY2(qAbs(player.position() - 1500) < 200, qPrintable(QString::number(player.position())));
        QVERIFY(player.paused());
        player.setAudioTracks({tracks[1].id}); QTest::qWait(200);
        QVERIFY(qAbs(player.position() - 1500) < 200); QVERIFY(player.paused());
        player.setAudioTracks({}); QTest::qWait(200);
        QVERIFY(qAbs(player.position() - 1500) < 200); QVERIFY(player.paused());
        player.pause(false); QTRY_VERIFY(!player.paused()); QTRY_VERIFY(player.position() > 1700);
        player.stop();
    }
    void initiallyPausedMixShowsVideo() {
        MpvPlayer player; player.resize(480, 270); player.show();
        bool sawTracks = false, metadataGap = false;
        connect(&player, &MpvPlayer::tracksChanged, this, [&] {
            const auto tracks = player.audioTracks();
            metadataGap |= sawTracks && tracks.isEmpty();
            if (tracks.size() != 2) return;
            sawTracks = true;
            player.setAudioTracks({tracks[0].id, tracks[1].id}); player.pause(true);
        });
        player.open(m_clip); QTRY_VERIFY(player.loaded()); QTRY_VERIFY(player.paused());
        QTRY_VERIFY_WITH_TIMEOUT(hasRedFrame(player), 5000);
        QTRY_VERIFY(!player.hardwareDecoder().isEmpty());
        qInfo().noquote() << "Playback hardware decoder:" << player.hardwareDecoder();
        QCOMPARE(player.position(), 0);
        QVERIFY(!metadataGap);
        player.stop();
    }
    void releaseCancelsPendingMixAndCanReopen() {
        MpvPlayer player; player.resize(480, 270); player.show(); player.open(m_clip);
        QTRY_COMPARE(player.audioTracks().size(), 2); QTRY_VERIFY(hasRedFrame(player));
        const auto tracks = player.audioTracks();
        player.setAudioTracks({tracks[0].id, tracks[1].id}); player.release();
        QVERIFY(player.source().isEmpty()); QVERIFY(!player.loaded());
        QTest::qWait(150); QVERIFY(player.source().isEmpty());
        player.open(m_clip); QTRY_VERIFY(player.loaded()); QTRY_VERIFY(hasRedFrame(player));
        player.release(); player.release();
        QVERIFY(player.source().isEmpty());
    }
};
QTEST_MAIN(PlaybackTests)
#include "tst_playback.moc"
