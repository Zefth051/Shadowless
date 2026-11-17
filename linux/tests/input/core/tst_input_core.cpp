#include "fake_input_source.h"
#include "input_renderer.h"
#include "input_timeline.h"
#include <QTemporaryDir>
#include "layout.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace Shadowless::Input;

class InputCoreTests : public QObject {
    Q_OBJECT
    static QJsonObject element(const QString &id, double x = 0, double y = 0) {
        return {{"id", id}, {"label", "A"}, {"shape", "round"}, {"x", x}, {"y", y}, {"w", 1}, {"h", 1}};
    }
    static QJsonObject doc(const QJsonArray &elements) {
        return {{"schema", 1}, {"id", "t"}, {"name", "T"}, {"scope", "gameplay"}, {"width", 4}, {"height", 4}, {"elements", elements}};
    }
private slots:
    void builtinLayoutsLoadAndAreConsistent() {
        QCOMPARE(Layout::builtinIds().size(), 5);
        for (const QString &id : Layout::builtinIds()) {
            Layout l; QString error;
            QVERIFY2(Layout::builtin(id, &l, &error), qPrintable(id + ": " + error));
            QCOMPARE(l.id(), id);
            QVERIFY(!l.allowlist().isEmpty());
            QSet<int> codes;
            for (const LayoutElement &e : l.elements())
                if (e.interactive() && e.evdev >= 0) QVERIFY2(!codes.contains(e.evdev), qPrintable(id + " duplicate evdev " + QString::number(e.evdev)));
                else if (e.interactive()) QVERIFY2(e.id.startsWith("pad.dpad") || e.id.startsWith("mouse."), qPrintable(e.id));
                else QVERIFY(true);
        }
        Layout none; QString error;
        QVERIFY(!Layout::builtin("none", &none, &error));
    }
    void scopeAndPrivacyFlags() {
        Layout full, compact, pad; QString e;
        QVERIFY(Layout::builtin("qwerty-full", &full, &e) && Layout::builtin("qwerty", &compact, &e) && Layout::builtin("dualsense", &pad, &e));
        QVERIFY(full.recordsTypingKeys());
        QVERIFY(!compact.recordsTypingKeys() && !pad.recordsTypingKeys());
        // Gameplay layouts must not expose the whole alphabet or Enter/Backspace.
        QVERIFY(!compact.allowlist().contains("key.enter") && !compact.allowlist().contains("key.backspace"));
        QVERIFY(compact.allowlist().size() < 30);
        // AZERTY shares physical ids with QWERTY; only labels differ.
        Layout az; QVERIFY(Layout::builtin("azerty", &az, &e));
        QCOMPARE(az.allowlist(), compact.allowlist());
    }
    void rejectsMalformedLayouts_data() {
        QTest::addColumn<QJsonObject>("json");
        auto bad = [](QJsonObject o, const char *k, QJsonValue v) { o[k] = v; return o; };
        const QJsonObject ok = doc({element("key.a")});
        QTest::newRow("schema") << bad(ok, "schema", 2);
        QTest::newRow("scope") << bad(ok, "scope", "x");
        QTest::newRow("id") << bad(ok, "id", "Bad Id");
        QTest::newRow("none id") << bad(ok, "id", "none");
        QTest::newRow("size") << bad(ok, "width", -1);
        QTest::newRow("empty") << bad(ok, "elements", QJsonArray());
        QTest::newRow("bad input id") << doc({element("text.hello")});
        QTest::newRow("upper input id") << doc({element("key.A")});
        QTest::newRow("duplicate") << doc({element("key.a"), element("key.a", 2)});
        QTest::newRow("out of bounds") << doc({element("key.a", 3.5)});
        QTest::newRow("negative") << doc({element("key.a", -1)});
        QJsonObject shape = element("key.a"); shape["shape"] = "evil";
        QTest::newRow("shape") << doc({shape});
        QJsonObject label = element("key.a"); label["label"] = "waytoolongg";
        QTest::newRow("label") << doc({label});
        QJsonObject decor = element(""); decor["shape"] = "padBody";
        QTest::newRow("no inputs") << doc({decor});
    }
    void rejectsMalformedLayouts() {
        QFETCH(QJsonObject, json);
        Layout l; QString error;
        QVERIFY(!Layout::fromJson(json, &l, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!l.isValid());
    }
    void stateTracksHeldIds() {
        InputState s;
        s.apply({"key.w", true, 1}); s.apply({"mouse.left", true, 2});
        QVERIFY(s.isDown("key.w") && s.isDown("mouse.left"));
        s.apply({"key.w", false, 3}); s.apply({"", true, 4});
        QVERIFY(!s.isDown("key.w") && s.pressed().size() == 1);
        s.clear(); QVERIFY(s.pressed().isEmpty());
    }
    void rendererShowsPressedKeysOnTransparentBackground() {
        Layout l; QString e; QVERIFY(Layout::builtin("qwerty", &l, &e));
        const QSize size(660, 340);
        InputState idle, held; held.apply({"key.w", true, 0});
        const QImage a = renderInputOverlay(l, idle, size), b = renderInputOverlay(l, held, size);
        QCOMPARE(a.size(), size);
        QCOMPARE(a.pixelColor(0, size.height() - 1).alpha(), 0); // corner stays transparent
        const double scale = qMin(size.width() / l.size().width(), size.height() / l.size().height());
        const QPointF origin((size.width() - l.size().width() * scale) / 2, (size.height() - l.size().height() * scale) / 2);
        QRectF w; for (const LayoutElement &el : l.elements()) if (el.id == "key.w") w = el.rect;
        const QPoint probe(int(origin.x() + (w.left() + .1) * scale), int(origin.y() + (w.top() + .1) * scale)); // away from the label
        QVERIFY(a.pixelColor(probe) != b.pixelColor(probe));
        QCOMPARE(b.pixelColor(probe).rgb(), RenderStyle().accent.rgb());
        QVERIFY(a != b);
        QCOMPARE(renderInputOverlay(l, held, size), b); // deterministic
    }
    void rendererHandlesDegenerateInput() {
        Layout l; QString e; QVERIFY(Layout::builtin("dualsense", &l, &e));
        QCOMPARE(renderInputOverlay(l, {}, QSize(0, 0)).size(), QSize(1, 1));
        QCOMPARE(renderInputOverlay(Layout(), {}, QSize(10, 10)).pixelColor(5, 5).alpha(), 0);
        RenderStyle s; s.opacity = 0;
        QCOMPARE(renderInputOverlay(l, {}, QSize(100, 60), s).pixelColor(50, 30).alpha(), 0);
    }
    void timelineReplaysEventsAndSeeksBackwards() {
        QJsonObject json{{"schema", 1}, {"layout", "qwerty"}, {"durationMs", 5000},
            {"events", QJsonArray{QJsonArray{900, "key.w", 0}, QJsonArray{100, "key.w", 1}, QJsonArray{300, "mouse.left", 1}}}};
        InputTimeline t; QString error;
        QVERIFY2(InputTimeline::fromJson(json, &t, &error), qPrintable(error));
        QCOMPARE(t.eventCount(), 3); QCOMPARE(t.durationMs(), qint64(5000)); QVERIFY(t.isValid());
        QVERIFY(t.stateAt(50).pressed().isEmpty());
        QVERIFY(t.stateAt(100).isDown("key.w"));           // events are sorted despite the input order
        QVERIFY(t.stateAt(400).isDown("mouse.left") && t.stateAt(400).isDown("key.w"));
        QVERIFY(!t.stateAt(900).isDown("key.w") && t.stateAt(900).isDown("mouse.left"));
        QVERIFY(t.stateAt(120).isDown("key.w") && !t.stateAt(120).isDown("mouse.left")); // backwards seek
        QVERIFY(t.stateAt(1 << 20).isDown("mouse.left"));
    }
    void timelineDropsIdsOutsideTheRecordedLayoutAndRejectsGarbage() {
        InputTimeline t; QString error;
        QJsonObject json{{"schema", 1}, {"layout", "qwerty"},
            {"events", QJsonArray{QJsonArray{10, "key.enter", 1}, QJsonArray{20, "key.w", 1}}}};
        QVERIFY(InputTimeline::fromJson(json, &t, &error));
        QCOMPARE(t.eventCount(), 1); // key.enter is not in the compact layout
        QVERIFY(!t.stateAt(100).isDown("key.enter"));
        auto bad = [&](QJsonObject o) { InputTimeline x; QString e; return InputTimeline::fromJson(o, &x, &e); };
        QVERIFY(!bad({{"schema", 2}, {"layout", "qwerty"}}));
        QVERIFY(!bad({{"schema", 1}, {"layout", "no-such-layout"}}));
        QVERIFY(!bad({{"schema", 1}, {"layout", "../x"}}));
        QVERIFY(!bad({{"schema", 1}, {"layout", "qwerty"}, {"events", QJsonArray{QJsonArray{1, "key.w"}}}}));
        QVERIFY(!bad({{"schema", 1}, {"layout", "qwerty"}, {"events", QJsonArray{QJsonArray{-1, "key.w", 1}}}}));
        QVERIFY(!bad({{"schema", 1}, {"layout", "qwerty"}, {"events", QJsonArray{QJsonArray{1, "key.w", 2}}}}));
        QVERIFY(!bad({{"schema", 1}, {"layout", "qwerty"}, {"events", QJsonArray{QJsonArray{1, "text.hello", 1}}}}));
        QJsonArray many; for (int i = 0; i <= InputTimeline::kMaxEvents; ++i) many.append(QJsonArray{i, "key.w", i % 2});
        QVERIFY(!bad({{"schema", 1}, {"layout", "qwerty"}, {"events", many}}));
    }
    void timelineLoadsFromFileAndNamesSidecarNextToClip() {
        QCOMPARE(InputTimeline::sidecarPath("/clips/a.mp4"), QString("/clips/a.mp4.inputs.json"));
        QTemporaryDir dir; const QString path = dir.filePath("c.mp4.inputs.json");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("{\"schema\":1,\"layout\":\"arrows\",\"events\":[[5,\"key.up\",1]]}"); f.close();
        InputTimeline t; QString error;
        QVERIFY2(InputTimeline::load(path, &t, &error), qPrintable(error));
        QCOMPARE(t.layoutId(), QString("arrows"));
        QVERIFY(t.stateAt(5).isDown("key.up"));
        QVERIFY(!InputTimeline::load(dir.filePath("missing.json"), &t, &error));
        QVERIFY(f.open(QIODevice::WriteOnly)); f.write("not json"); f.close();
        QVERIFY(!InputTimeline::load(path, &t, &error));
    }
    void fakeSourceEnforcesAllowlistAndReleasesOnStop() {
        qRegisterMetaType<InputEvent>(); qRegisterMetaType<InputStatus>();
        FakeInputSource src;
        QSignalSpy events(&src, &InputSource::inputChanged), status(&src, &InputSource::statusChanged);
        QVERIFY(!src.inject("key.w", true)); // not started
        src.start({"key.w"});
        QCOMPARE(src.status(), InputStatus::Active);
        QVERIFY(src.inject("key.w", true, 5));
        QVERIFY(!src.inject("key.enter", true)); // outside allowlist: never emitted
        src.stop();
        QCOMPARE(events.size(), 2); // down + synthetic up
        QVERIFY(!events.last().first().value<InputEvent>().down);
        QCOMPARE(src.status(), InputStatus::Stopped);
        QCOMPARE(status.size(), 2);
        src.stop(); QCOMPARE(status.size(), 2); // idempotent
    }
};
QTEST_MAIN(InputCoreTests)
#include "tst_input_core.moc"
