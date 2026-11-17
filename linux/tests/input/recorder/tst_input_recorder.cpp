#include "fake_input_source.h"
#include "input_recorder.h"
#include "input_timeline.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace Shadowless::Input;

namespace {
QJsonObject readSidecar(const QString &clip) {
    QFile file(InputTimeline::sidecarPath(clip));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QJsonArray edge(qint64 time, const QString &id, int down) {
    return QJsonArray{time, id, down};
}
}

class RecorderTests : public QObject {
    Q_OBJECT
private slots:
    void replayWindow();
    void continuous_data();
    void continuous();
    void allowlistAndStop();
    void layoutFiltering_data();
    void layoutFiltering();
    void retentionEviction();
    void idleRetention();
    void capAndTruncated();
    void syntheticEventsRespectCap();
    void invalidRequests();
    void atomicFailures();
    void atomicReplacement();
};

void RecorderTests::replayWindow() {
    QTemporaryDir dir;
    const QString clip = dir.filePath("replay.mp4");
    FakeInputSource source;
    source.start({"key.w", "key.a", "key.s"});
    InputRecorder recorder(&source, "qwerty", 1000);
    recorder.start();
    const qint64 t = monotonicMs();
    source.inject("key.w", true, t);
    source.inject("key.a", true, t + 9);
    source.inject("key.a", false, t + 10);
    source.inject("key.w", false, t + 20);
    source.inject("key.s", true, t + 30);
    source.inject("key.s", false, t + 31);
    QString error;
    QVERIFY2(recorder.writeWindow(clip, t + 10, t + 30, &error), qPrintable(error));
    const QJsonArray expected{edge(0, "key.a", 1), edge(0, "key.w", 1),
                              edge(0, "key.a", 0), edge(10, "key.w", 0), edge(20, "key.s", 1)};
    QCOMPARE(readSidecar(clip), (QJsonObject{{"schema", 1}, {"layout", "qwerty"},
                                           {"durationMs", 20}, {"events", expected}}));
    InputTimeline timeline;
    QVERIFY2(InputTimeline::load(InputTimeline::sidecarPath(clip), &timeline, &error), qPrintable(error));
    QCOMPARE(timeline.durationMs(), 20);
    QCOMPARE(timeline.layoutId(), QString("qwerty"));
    QCOMPARE(timeline.eventCount(), 5);
    QVERIFY(timeline.stateAt(0).isDown("key.w"));
    QVERIFY(!timeline.stateAt(0).isDown("key.a"));
    QVERIFY(!timeline.stateAt(10).isDown("key.w"));
    QVERIFY(timeline.stateAt(20).isDown("key.s"));
}

void RecorderTests::continuous_data() {
    QTest::addColumn<qint64>("retention");
    QTest::newRow("zero") << qint64(0);
    QTest::newRow("negative") << qint64(-1);
}

void RecorderTests::continuous() {
    QFETCH(qint64, retention);
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({"key.w"});
    InputRecorder recorder(&source, "qwerty", retention);
    recorder.start();
    const qint64 t = monotonicMs();
    source.inject("key.w", true, t);
    source.inject("key.w", false, t + 100000);
    recorder.stop();
    const QString clip = dir.filePath("recording.mp4");
    QString error;
    QVERIFY2(recorder.writeWindow(clip, t, t + 100000, &error), qPrintable(error));
    QCOMPARE(readSidecar(clip).value("events").toArray(),
             (QJsonArray{edge(0, "key.w", 1), edge(100000, "key.w", 0)}));
    InputTimeline timeline;
    QVERIFY(InputTimeline::load(InputTimeline::sidecarPath(clip), &timeline, &error));
    QVERIFY(timeline.stateAt(99999).isDown("key.w"));
    QVERIFY(!timeline.stateAt(100000).isDown("key.w"));
}

void RecorderTests::allowlistAndStop() {
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({"key.w", "key.p", "private.identifier"});
    InputRecorder recorder(&source, "qwerty", 0);
    const qint64 t = monotonicMs();
    source.inject("key.w", true, t);
    recorder.start();
    recorder.start();
    source.inject("key.p", true, t + 1);
    source.inject("private.identifier", true, t + 2);
    source.inject("key.w", true, t + 3);
    recorder.stop();
    recorder.stop();
    source.inject("key.w", false, t + 4);
    QCoreApplication::processEvents();
    QString error;
    const QString clip = dir.filePath("allowed.mp4");
    QVERIFY(recorder.writeWindow(clip, t, t + 10, &error));
    QCOMPARE(readSidecar(clip).value("events").toArray(), (QJsonArray{edge(3, "key.w", 1)}));
    QCOMPARE(source.status(), InputStatus::Active);
}

void RecorderTests::layoutFiltering_data() {
    QTest::addColumn<QString>("layout");
    QTest::addColumn<QString>("allowed");
    QTest::addColumn<QString>("excluded");
    QTest::newRow("qwerty") << QString("qwerty") << QString("key.w") << QString("key.p");
    QTest::newRow("full") << QString("qwerty-full") << QString("key.p") << QString("pad.south");
    QTest::newRow("arrows") << QString("arrows") << QString("key.up") << QString("key.w");
    QTest::newRow("azerty") << QString("azerty") << QString("key.w") << QString("key.p");
    QTest::newRow("pad") << QString("dualsense") << QString("pad.south") << QString("key.w");
}

void RecorderTests::layoutFiltering() {
    QFETCH(QString, layout);
    QFETCH(QString, allowed);
    QFETCH(QString, excluded);
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({allowed, excluded});
    InputRecorder recorder(&source, layout, 0);
    recorder.start();
    const qint64 t = monotonicMs();
    source.inject(allowed, true, t);
    source.inject(excluded, true, t);
    const QString clip = dir.filePath("layout.mp4");
    QString error;
    QVERIFY2(recorder.writeWindow(clip, t, t, &error), qPrintable(error));
    QCOMPARE(readSidecar(clip).value("events").toArray(), (QJsonArray{edge(0, allowed, 1)}));
    InputTimeline timeline;
    QVERIFY2(InputTimeline::load(InputTimeline::sidecarPath(clip), &timeline, &error), qPrintable(error));
    QCOMPARE(timeline.layoutId(), layout);
    QVERIFY(timeline.stateAt(0).isDown(allowed));
    QVERIFY(!timeline.stateAt(0).isDown(excluded));
}

void RecorderTests::retentionEviction() {
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({"key.w", "key.a"});
    InputRecorder recorder(&source, "qwerty", 100);
    recorder.start();
    const qint64 t = monotonicMs();
    source.inject("key.w", true, t);
    source.inject("key.a", true, t + 99);
    source.inject("key.a", false, t + 100);
    source.inject("key.w", false, t + 200);
    QString error;
    const QString clip = dir.filePath("retained.mp4");
    QVERIFY(recorder.writeWindow(clip, t + 100, t + 200, &error));
    QCOMPARE(readSidecar(clip).value("events").toArray(),
             (QJsonArray{edge(0, "key.a", 1), edge(0, "key.w", 1),
                         edge(0, "key.a", 0), edge(100, "key.w", 0)}));
    QVERIFY(!readSidecar(clip).value("truncated").toBool());
    // Asking for older history must not resurrect evicted edges.
    QVERIFY(recorder.writeWindow(clip, t, t + 200, &error));
    const QJsonArray events = readSidecar(clip).value("events").toArray();
    QVERIFY(!events.contains(edge(99, "key.a", 1)));
}

void RecorderTests::idleRetention() {
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({"key.w"});
    InputRecorder recorder(&source, "qwerty", 10);
    recorder.start();
    const qint64 t = monotonicMs() - 1000;
    source.inject("key.w", true, t);
    source.inject("key.w", false, t + 1);
    const QString clip = dir.filePath("idle.mp4");
    QString error;
    QVERIFY(recorder.writeWindow(clip, t, monotonicMs(), &error));
    QVERIFY(readSidecar(clip).value("events").toArray().isEmpty());
}

void RecorderTests::capAndTruncated() {
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({"key.w"});
    InputRecorder recorder(&source, "qwerty", 0);
    recorder.start();
    const qint64 t = monotonicMs();
    for (int i = 0; i < 200002; ++i) source.inject("key.w", i % 2 == 0, t + i);
    const QString clip = dir.filePath("cap.mp4");
    QString error;
    QVERIFY2(recorder.writeWindow(clip, t, t + 200002, &error), qPrintable(error));
    const QJsonArray events = readSidecar(clip).value("events").toArray();
    QCOMPARE(events.size(), 200000);
    QCOMPARE(events.first(), edge(2, "key.w", 1));
    QVERIFY(readSidecar(clip).value("truncated").toBool());
    InputTimeline timeline;
    QVERIFY2(InputTimeline::load(InputTimeline::sidecarPath(clip), &timeline, &error), qPrintable(error));
    QVERIFY(timeline.truncated());
    QCOMPARE(timeline.eventCount(), 200000);
}

void RecorderTests::syntheticEventsRespectCap() {
    QTemporaryDir dir;
    FakeInputSource source;
    source.start({"key.w", "key.a"});
    InputRecorder recorder(&source, "qwerty", 0);
    recorder.start();
    const qint64 t = monotonicMs();
    source.inject("key.a", true, t);
    for (int i = 0; i < 200000; ++i) source.inject("key.w", i % 2 == 0, t + 1 + i);
    const QString clip = dir.filePath("synthetic-cap.mp4");
    QString error;
    QVERIFY(recorder.writeWindow(clip, t + 1, t + 200001, &error));
    InputTimeline timeline;
    QVERIFY2(InputTimeline::load(InputTimeline::sidecarPath(clip), &timeline, &error), qPrintable(error));
    QVERIFY(timeline.truncated());
    QCOMPARE(timeline.eventCount(), 200000);
    QVERIFY(timeline.stateAt(0).isDown("key.a"));
}

void RecorderTests::invalidRequests() {
    QTemporaryDir dir;
    FakeInputSource source;
    InputRecorder unknown(&source, "unknown", 0);
    unknown.start();
    QString error;
    const QString clip = dir.filePath("invalid.mp4");
    QVERIFY(!unknown.writeWindow(clip, 1, 2, &error));
    QVERIFY(error.contains("layout"));
    InputRecorder recorder(&source, "qwerty", 0);
    QVERIFY(!recorder.writeWindow(clip, 2, 1, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!recorder.writeWindow(clip, -1, 1, &error));
    QVERIFY(!QFile::exists(InputTimeline::sidecarPath(clip)));
    QVERIFY(recorder.writeWindow(clip, 1, 1, nullptr));
    QCOMPARE(readSidecar(clip).value("durationMs").toInteger(), 0);
}

void RecorderTests::atomicFailures() {
    QTemporaryDir dir;
    FakeInputSource source;
    InputRecorder recorder(&source, "qwerty", 0);
    const auto permissions = QFile::permissions(dir.path());
    QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner));
    QString error;
    const bool written = recorder.writeWindow(dir.filePath("readonly.mp4"), 1, 2, &error);
    const QStringList leftovers = QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    QVERIFY(QFile::setPermissions(dir.path(), permissions));
    QVERIFY(!written);
    QVERIFY(!error.isEmpty());
    QVERIFY(leftovers.isEmpty());
    // A destination directory exercises failure without leaving temporary files.
    QVERIFY(QDir().mkdir(dir.filePath("blocked.mp4.inputs.json")));
    QVERIFY(!recorder.writeWindow(dir.filePath("blocked.mp4"), 1, 2, &error));
    QCOMPARE(QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot),
             QStringList{"blocked.mp4.inputs.json"});
    QVERIFY(!recorder.writeWindow(dir.filePath("missing/clip.mp4"), 1, 2, &error));
}

void RecorderTests::atomicReplacement() {
    QTemporaryDir dir;
    FakeInputSource source;
    InputRecorder recorder(&source, "qwerty", 0);
    const QString clip = dir.filePath("replace.mp4");
    QString error;
    QVERIFY(recorder.writeWindow(clip, 1, 2, &error));
    QVERIFY(recorder.writeWindow(clip, 1, 3, &error));
    QCOMPARE(readSidecar(clip).value("durationMs").toInteger(), 2);
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden), QStringList{"replace.mp4.inputs.json"});
}

QTEST_GUILESS_MAIN(RecorderTests)
#include "tst_input_recorder.moc"
