#include "fake_input_source.h"
#include "input_sidecar_session.h"
#include "input_timeline.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace Shadowless::Input;
using Outcome = InputSidecarSession::Outcome;

namespace {
struct Fixture {
    QTemporaryDir dir;
    FakeInputSource source;
    qint64 now = 0;                 // fake session clock
    qint64 base = monotonicMs();    // events use real monotonic time because the recorder evicts against it
    std::unique_ptr<InputSidecarSession> session;
    Fixture(const QSet<InputId> &allow = {"key.w", "key.a", "key.space", "pad.south"}) {
        source.start(allow);
        session = std::make_unique<InputSidecarSession>(&source, [this] { return now; });
    }
    InputSidecarSession::State state(const QString &layout = "qwerty", bool capture = true, bool recording = false, qint64 buffer = 30000) const {
        InputSidecarSession::State s; s.enabled = true; s.layout = layout; s.captureActive = capture; s.recordingOutput = recording; s.bufferMs = buffer; return s;
    }
    QString clip(const char *name = "clip.mp4") const { return dir.filePath(name); }
    // Session clock and event clock share one origin so test numbers read as seconds on the same timeline.
    qint64 at(qint64 ms) const { return base + ms; }
    QJsonObject read(const QString &clip) const {
        QFile f(InputTimeline::sidecarPath(clip));
        if (!f.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(f.readAll()).object();
    }
    bool exists(const QString &clip) const { return QFileInfo::exists(InputTimeline::sidecarPath(clip)); }
};
QJsonArray ev(qint64 t, const QString &id, int down) { return QJsonArray{t, id, down}; }
} // namespace

class SidecarTests : public QObject {
    Q_OBJECT
private slots:
    void replayClipCoversTheBufferEndingAtTheSaveRequest();
    void replayClipIsShorterWhenCaptureJustStarted();
    void missingSaveRequestFallsBackToNow();
    void continuousRecordingEndsAtTheStopRequestNotAtFinalization();
    void recordingNextToReplayStartsAtItsFirstFrame();
    void pausedRecordingGetsNoSidecarUntilANewOneStarts();
    void layoutChangeStartsAnEmptyHistory();
    void retentionFollowsReplayOnlyVersusRecording();
    void inactiveOrDisabledSessionRecordsNothing();
    void updateIsIdempotentAndSubscribesOnce();
    void enablingMidCaptureCoversOnlyLaterInput();
    void shutdownIsSafeInAnyOrder();
    void writeFailureLeavesNoPartialFile();
    void allowlistFollowsTheActiveLayout();
};

void SidecarTests::replayClipCoversTheBufferEndingAtTheSaveRequest() {
    Fixture f; const auto s = f.state();
    f.now = f.at(0); f.session->update(s); f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(50000)); f.source.inject("key.w", false, f.at(90000));   // outside the last 30 s
    f.source.inject("key.a", true, f.at(100000));                                                  // inside, still held at the end
    f.source.inject("key.space", true, f.at(120000)); f.source.inject("key.space", false, f.at(125000));
    f.now = f.at(130000); f.session->replaySaveRequested();
    f.now = f.at(133000);                                                                          // the save finished later
    QString error;
    QCOMPARE(f.session->writeClip(f.clip(), false, &error), Outcome::Written);
    const auto json = f.read(f.clip());
    QCOMPARE(json.value("durationMs").toInt(), 30000);
    QCOMPARE(json.value("events").toArray(), (QJsonArray{ev(0, "key.a", 1), ev(20000, "key.space", 1), ev(25000, "key.space", 0)}));
    InputTimeline timeline; QVERIFY2(InputTimeline::load(InputTimeline::sidecarPath(f.clip()), &timeline, &error), qPrintable(error));
    QVERIFY(timeline.stateAt(0).isDown("key.a")); QVERIFY(timeline.stateAt(20000).isDown("key.space"));
}

void SidecarTests::replayClipIsShorterWhenCaptureJustStarted() {
    Fixture f; f.now = f.at(100000); f.session->update(f.state()); f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(104000));
    f.now = f.at(112000); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Written);
    const auto json = f.read(f.clip());
    QCOMPARE(json.value("durationMs").toInt(), 12000);   // 12 s of capture, not the 30 s buffer
    QCOMPARE(json.value("events").toArray(), (QJsonArray{ev(4000, "key.w", 1)}));
}

void SidecarTests::missingSaveRequestFallsBackToNow() {
    Fixture f; f.now = f.at(0); f.session->update(f.state()); f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(8000));
    f.now = f.at(10000);                                  // no replaySaveRequested(): the clip is assumed to end now
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Written);
    QCOMPARE(f.read(f.clip()).value("durationMs").toInt(), 10000);
    // Without a capture start there is no way to place the clip: skip rather than guess.
    Fixture g; g.session->update(g.state()); g.now = g.at(1000);
    QCOMPARE(g.session->writeClip(g.clip(), false), Outcome::Skipped);
    QVERIFY(!g.exists(g.clip()));
}

void SidecarTests::continuousRecordingEndsAtTheStopRequestNotAtFinalization() {
    Fixture f; const auto s = f.state("qwerty", true, true);
    f.now = f.at(1000); f.session->update(s); f.session->captureStarted(true);
    f.source.inject("key.w", true, f.at(2000)); f.source.inject("key.w", false, f.at(3000));
    f.source.inject("key.a", true, f.at(66000));          // pressed after the user already asked to stop
    f.now = f.at(61000); f.session->recordingStopRequested();
    f.now = f.at(65000);                                  // finalization took 4 s
    QCOMPARE(f.session->writeClip(f.clip(), true), Outcome::Written);
    const auto json = f.read(f.clip());
    QCOMPARE(json.value("durationMs").toInt(), 60000);
    QCOMPARE(json.value("events").toArray(), (QJsonArray{ev(1000, "key.w", 1), ev(2000, "key.w", 0)}));
}

void SidecarTests::recordingNextToReplayStartsAtItsFirstFrame() {
    Fixture f; const auto replay = f.state(); const auto both = f.state("qwerty", true, true);
    f.now = f.at(0); f.session->update(replay); f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(5000)); f.source.inject("key.w", false, f.at(6000));
    f.now = f.at(10000); f.session->recordingRequested(); f.session->update(both); f.now = f.at(10400); f.session->recordingStarted();
    f.source.inject("key.a", true, f.at(11000)); f.source.inject("key.a", false, f.at(12000));
    f.now = f.at(40000); f.session->recordingStopRequested(); f.now = f.at(42000);
    QCOMPARE(f.session->writeClip(f.clip("rec.mp4"), true), Outcome::Written);
    const auto json = f.read(f.clip("rec.mp4"));
    QCOMPARE(json.value("durationMs").toInt(), 29600);    // from the first frame (10.4 s), not from the request
    QCOMPARE(json.value("events").toArray(), (QJsonArray{ev(600, "key.a", 1), ev(1600, "key.a", 0)}));
}

void SidecarTests::pausedRecordingGetsNoSidecarUntilANewOneStarts() {
    Fixture f; f.now = f.at(0); f.session->update(f.state("qwerty", true, true)); f.session->captureStarted(true);
    f.source.inject("key.w", true, f.at(1000));
    f.now = f.at(5000); f.session->recordingPaused();
    f.now = f.at(20000); f.session->recordingStopRequested();
    QCOMPARE(f.session->writeClip(f.clip("paused.mp4"), true), Outcome::Skipped);
    QVERIFY(!f.exists(f.clip("paused.mp4")));             // paused time is cut from the video: events would drift
    f.now = f.at(30000); f.session->recordingStarted();    // a fresh recording is aligned again
    f.source.inject("key.a", true, f.at(31000));
    f.now = f.at(40000); f.session->recordingStopRequested();
    QCOMPARE(f.session->writeClip(f.clip("next.mp4"), true), Outcome::Written);
    // key.w was pressed during the paused recording and is still held when the new one starts: reported as down at 0.
    QCOMPARE(f.read(f.clip("next.mp4")).value("events").toArray(), (QJsonArray{ev(0, "key.w", 1), ev(1000, "key.a", 1)}));
    // The replay clip of the same capture is unaffected by the pause flag.
    f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip("replay.mp4"), false), Outcome::Written);
}

void SidecarTests::layoutChangeStartsAnEmptyHistory() {
    Fixture f({"key.w", "pad.south"}); f.now = f.at(0);
    f.session->update(f.state("qwerty")); f.session->captureStarted(false);
    QVERIFY(f.session->allowlist().contains("key.w") && !f.session->allowlist().contains("pad.south"));
    f.source.inject("key.w", true, f.at(1000)); f.source.inject("key.w", false, f.at(1500));
    f.now = f.at(5000); f.session->update(f.state("dualsense"));   // new layout: old history is dropped, never mixed
    QVERIFY(f.session->allowlist().contains("pad.south") && !f.session->allowlist().contains("key.w"));
    f.source.inject("key.w", true, f.at(6000));                    // not part of the gamepad layout
    f.source.inject("pad.south", true, f.at(7000)); f.source.inject("pad.south", false, f.at(7500));
    f.now = f.at(10000); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Written);
    const auto json = f.read(f.clip());
    QCOMPARE(json.value("layout").toString(), QString("dualsense"));
    QCOMPARE(json.value("events").toArray(), (QJsonArray{ev(7000, "pad.south", 1), ev(7500, "pad.south", 0)}));
}

void SidecarTests::retentionFollowsReplayOnlyVersusRecording() {
    // Events arrive in chronological order, as from a real source; eviction is driven by event time.
    Fixture f; f.now = f.at(0);
    f.session->update(f.state("qwerty", true, false, 30000)); f.session->captureStarted(false);     // keeps buffer + 10 s = 40 s
    f.source.inject("key.w", true, f.at(1000)); f.source.inject("key.w", false, f.at(2000));
    f.source.inject("key.a", true, f.at(100000)); f.source.inject("key.a", false, f.at(101000));    // pushes key.w out of the 40 s window
    f.now = f.at(101500); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip("a.mp4"), false), Outcome::Written);
    QCOMPARE(f.read(f.clip("a.mp4")).value("events").toArray(), (QJsonArray{ev(28500, "key.a", 1), ev(29500, "key.a", 0)}));
    // A recording needs everything since its first frame: nothing is evicted while it runs, however long it takes.
    f.session->update(f.state("qwerty", true, true, 400000));
    f.source.inject("key.space", true, f.at(110000)); f.source.inject("key.space", false, f.at(111000));
    f.source.inject("key.w", true, f.at(300000)); f.source.inject("key.w", false, f.at(301000));
    f.now = f.at(302000); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip("b.mp4"), false), Outcome::Written);
    QCOMPARE(f.read(f.clip("b.mp4")).value("events").toArray().size(), 6);                           // key.a, key.space and the new key.w
    // Back to replay-only: older history is evicted at once, even without new input.
    f.session->update(f.state("qwerty", true, false, 30000));
    f.session->update(f.state("qwerty", true, false, 400000));                                       // widen the window to see what is left
    QCOMPARE(f.session->writeClip(f.clip("c.mp4"), false), Outcome::Written);
    QCOMPARE(f.read(f.clip("c.mp4")).value("events").toArray().size(), 2);                           // only the latest key.w pair survived
}

void SidecarTests::inactiveOrDisabledSessionRecordsNothing() {
    Fixture f; f.now = f.at(0);
    auto off = f.state(); off.enabled = false;
    f.session->update(off); f.session->captureStarted(false);
    QVERIFY(!f.session->active() && f.session->allowlist().isEmpty());
    f.source.inject("key.w", true, f.at(100)); f.now = f.at(1000); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Skipped);
    QVERIFY(!f.exists(f.clip()));
    f.session->update(f.state("qwerty", false));                                                   // capture not running
    QVERIFY(!f.session->active());
    f.session->update(f.state("qwerty", true)); QVERIFY(f.session->active());
    f.session->update(f.state("qwerty", false)); QVERIFY(!f.session->active());                  // engine stopped: history dropped
    QVERIFY(f.session->allowlist().isEmpty());
}

void SidecarTests::updateIsIdempotentAndSubscribesOnce() {
    Fixture f; f.now = f.at(0);
    for (int i = 0; i < 5; ++i) f.session->update(f.state());
    f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(2000));
    f.now = f.at(5000); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Written);
    QCOMPARE(f.read(f.clip()).value("events").toArray(), (QJsonArray{ev(2000, "key.w", 1)}));    // one event, not five
}

void SidecarTests::enablingMidCaptureCoversOnlyLaterInput() {
    Fixture f; f.now = f.at(0);
    auto off = f.state(); off.enabled = false;
    f.session->update(off); f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(2000)); f.source.inject("key.w", false, f.at(3000));      // before the switch
    f.now = f.at(10000); f.session->update(f.state());
    f.source.inject("key.a", true, f.at(12000)); f.source.inject("key.a", false, f.at(13000));
    f.now = f.at(20000); f.session->replaySaveRequested();
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Written);
    QCOMPARE(f.read(f.clip()).value("events").toArray(), (QJsonArray{ev(12000, "key.a", 1), ev(13000, "key.a", 0)}));
}

void SidecarTests::shutdownIsSafeInAnyOrder() {
    {   // source releases held keys while stopping; the session must not record or crash
        Fixture f; f.now = f.at(0); f.session->update(f.state()); f.session->captureStarted(false);
        f.source.inject("key.w", true, f.at(100));
        f.session->shutdown(); f.session->shutdown();                                              // idempotent
        f.source.stop();                                                                           // emits the release edge
        QVERIFY(!f.session->active());
        f.session->update(f.state());                                                              // closed for good
        QVERIFY(!f.session->active());
        QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Skipped);
    }
    {   // the source disappears first
        auto *source = new FakeInputSource; source->start({"key.w"});
        InputSidecarSession session(source, [] { return qint64(0); });
        InputSidecarSession::State s; s.enabled = true; s.layout = "qwerty"; s.captureActive = true; s.bufferMs = 1000;
        session.update(s); QVERIFY(session.active());
        delete source;
        session.update(s);                                                                         // no source: drops the recorder
        QVERIFY(!session.active());
        session.shutdown();
    }
    {   // session destroyed while the source keeps emitting
        FakeInputSource source; source.start({"key.w"});
        auto session = std::make_unique<InputSidecarSession>(&source, [] { return qint64(0); });
        InputSidecarSession::State s; s.enabled = true; s.layout = "qwerty"; s.captureActive = true;
        session->update(s); session.reset();
        QVERIFY(source.inject("key.w", true));                                                    // must not touch freed memory
    }
}

void SidecarTests::writeFailureLeavesNoPartialFile() {
    Fixture f; f.now = f.at(0); f.session->update(f.state()); f.session->captureStarted(false);
    f.source.inject("key.w", true, f.at(1000)); f.now = f.at(5000); f.session->replaySaveRequested();
    const QString missing = f.dir.filePath("no-such-folder/clip.mp4");
    QString error;
    QCOMPARE(f.session->writeClip(missing, false, &error), Outcome::Failed);
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(InputTimeline::sidecarPath(missing)));
    QVERIFY(QDir(f.dir.path()).entryList(QDir::Files | QDir::Hidden).isEmpty());                    // no stray temp files
    QCOMPARE(f.session->writeClip(f.clip(), false), Outcome::Written);                              // and the session keeps working
}

void SidecarTests::allowlistFollowsTheActiveLayout() {
    QVERIFY(InputSidecarSession::allowlistFor("qwerty").contains("key.w"));
    QVERIFY(InputSidecarSession::allowlistFor("unknown-layout").isEmpty());
    QVERIFY(InputSidecarSession::allowlistFor("none").isEmpty());
    QCOMPARE(InputSidecarSession::allowlistFor("azerty"), InputSidecarSession::allowlistFor("qwerty"));
}

QTEST_MAIN(SidecarTests)
#include "tst_input_sidecar.moc"
