#include "../ui/dashboard.h"
#include "../ui/library.h"
#include "../ui/settings_page.h"
#include "../ui/overlay_editor.h"
#include "../ui/status_indicator.h"
#include "../ui/clip_player.h"
#include "../ui/clip_waveform.h"
#include "../player/mpv_player.h"
#include "../player/media_tools.h"
#include "../app/controller.h"
#include "../capture/replay_paths.h"
#include "../ui/clip_notification.h"
#include "../ui/theme.h"
#include "../ui/input_overlay_preview.h"
#include "../ui/input_overlay_layer.h"
#include "../ui/input_overlay_panel.h"
#include <QCheckBox>
#include <QTableWidget>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractItemView>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QMenu>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <clocale>
#include <cmath>
#include <QScopeGuard>
#include <QDesktopServices>
#include <QUrl>
#include <QLayout>
#include "../ui/input_access_dialog.h"

using namespace Shadowless;
Q_DECLARE_METATYPE(Shadowless::GsrOptions)
Q_DECLARE_METATYPE(Shadowless::OverlayScene)

class UiTests : public QObject {
    Q_OBJECT
    const GsrCapabilities caps{"6.1.5", "nvidia", "wayland", {"h264", "hevc"}, {"DP-1"}};
    const QStringList keys{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10", "Alt+F9"};
    QPushButton *button(QWidget *parent, const QString &text) {
        for (auto *candidate : parent->findChildren<QPushButton *>()) if (candidate->text() == text) return candidate;
        return nullptr;
    }
    QUrl m_openedUrl;
private slots:
    void captureOpenedUrl(const QUrl &url) { m_openedUrl = url; }
    void authenticationReleasesOverlayAndRestoresVisibility() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        Controller controller(caps, {}, false);
        auto *engine = controller.findChild<GsrCapture *>(); QVERIFY(engine);
        Dashboard *dashboard = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *candidate = qobject_cast<Dashboard *>(widget)) dashboard = candidate;
        QVERIFY(dashboard); QVERIFY(dashboard->isVisible());
        const auto auth = [engine](bool active) {
            return QMetaObject::invokeMethod(engine, "authenticationChanged", Q_ARG(bool, active));
        };
        QVERIFY(auth(true)); QVERIFY(!dashboard->isVisible());
        controller.command("show"); QVERIFY(!dashboard->isVisible());
        QVERIFY(auth(false)); QVERIFY(dashboard->isVisible());
        controller.command("hide");
        QVERIFY(auth(true)); QVERIFY(auth(false)); QVERIFY(!dashboard->isVisible());
        controller.command("show"); QVERIFY(auth(true));
        controller.command("hide"); QVERIFY(auth(false)); QVERIFY(!dashboard->isVisible());
    }
    void activeCaptureNoticeStaysVisibleAndFieldsDim() {
        SettingsPage page(caps); applyTheme(&page, "violet");
        page.resize(560, 650); page.edit(GsrOptions{}, false, true, keys); page.show();
        auto *notice = page.findChild<QLabel *>("captureActiveNotice"); QVERIFY(notice);
        auto *fields = page.findChild<QWidget *>("captureSettings"); QVERIFY(fields);
        QVERIFY(!fields->graphicsEffect()); // graphics effects over scrolled widgets crash on Plasma/Wayland
        QVERIFY(!fields->isEnabled());
        page.showAudio(); QTest::qWait(20);
        QVERIFY(notice->isVisible()); QVERIFY(page.rect().contains(notice->mapTo(&page, QPoint())));
        page.setCaptureActive(false); QVERIFY(fields->isEnabled());
        QVERIFY(!notice->isVisible());
    }
    void dynamicApplicationControlsKeepExistingChoices() {
        Dashboard dashboard(caps); GsrOptions options; options.dynamicApplicationAudio=true;
        dashboard.updateState(options,{},true,true,false,false); dashboard.show();
        // The main menu shows only the PC / Game / Microphone groups, never per-application rows.
        QVERIFY(dashboard.findChildren<QPushButton *>("applicationTrack").isEmpty());
        QVERIFY(dashboard.findChildren<QPushButton *>("applicationGame").isEmpty());
        const auto summary=dashboard.findChild<QLabel *>("audioTrackSummary")->text();
        QVERIFY(summary.contains("PC")); QVERIFY(summary.contains("Game"));
        QVERIFY(!dashboard.findChild<QPushButton *>("fourTrackSetup")->isVisible());
        QTemporaryDir directory; Settings settings(directory.filePath("settings.ini")); QString error;
        options.applicationIncluded={{"Spotify",false}}; options.applicationGame={{"Late browser",true}};
        QVERIFY(settings.save(options,{},false,&error)); const auto loaded=settings.capture(caps);
        QCOMPARE(loaded.applicationIncluded,options.applicationIncluded); QCOMPARE(loaded.applicationGame,options.applicationGame);
        QVERIFY(loaded.dynamicAudioEnabled());
    }
    void settingsRowsFitNarrowPanelAndSwitchFromKeyboard() {
        SettingsPage page(caps); applyTheme(&page);
        GsrOptions options; options.source = "DP-1"; options.bufferSeconds = 60;
        page.edit(options, false, false, keys);
        auto *largeLabel=page.findChild<QCheckBox *>("organizeByGame"); QVERIFY(largeLabel);
        largeLabel->setStyleSheet("font-family: monospace; font-size: 18px;");
        for (const auto size : {QSize(420, 620), QSize(560, 740)}) {
            page.resize(size); page.show(); QCoreApplication::processEvents();
            auto *scroll = page.findChild<QScrollArea *>(); QVERIFY(scroll);
            QVERIFY2(scroll->widget()->width() <= scroll->viewport()->width(),
                     qPrintable(QString("Settings body width %1 exceeds viewport %2").arg(scroll->widget()->width()).arg(scroll->viewport()->width())));
            auto *toggle = page.findChild<QCheckBox *>("organizeByGame"); QVERIFY(toggle);
            scroll->ensureWidgetVisible(toggle); QCoreApplication::processEvents();
            QVERIFY(toggle->width() > 250);
            const bool previous = toggle->isChecked();
            QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(toggle->width() - 10, toggle->height() / 2));
            QCOMPARE(toggle->isChecked(), !previous);
            if (toggle->isChecked())
                QCOMPARE(toggle->grab().toImage().pixelColor(toggle->width() - 35, toggle->height() / 2), accentColor("violet"));
            toggle->setFocus(); QTest::keyClick(toggle, Qt::Key_Space);
            QCOMPARE(toggle->isChecked(), previous);
            scroll->verticalScrollBar()->setValue(0);
            const auto output = qEnvironmentVariable("SHADOWLESS_SETTINGS_SCREENSHOT");
            if (!output.isEmpty()) QVERIFY(page.grab().save(output + QString::number(size.width()) + ".png"));
        }
    }
    void shortcutApprovalReleasesOverlayAndRestoresVisibility() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        Controller controller(caps, {}, false);
        auto *shortcuts = controller.findChild<Shortcuts *>();
        QVERIFY(shortcuts);
        Dashboard *dashboard = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *candidate = qobject_cast<Dashboard *>(widget)) dashboard = candidate;
        QVERIFY(dashboard); QVERIFY(dashboard->isVisible());
        const auto approval = [shortcuts](bool pending) {
            return QMetaObject::invokeMethod(shortcuts, "approvalChanged", Q_ARG(bool, pending));
        };
        QVERIFY(approval(true)); QVERIFY(!dashboard->isVisible());
        controller.command("show"); QVERIFY(!dashboard->isVisible());
        QVERIFY(approval(false)); QVERIFY(dashboard->isVisible());
        auto *engine = controller.findChild<GsrCapture *>(); QVERIFY(engine);
        QVERIFY(approval(true));
        QVERIFY(QMetaObject::invokeMethod(engine, "authenticationChanged", Q_ARG(bool, true)));
        QVERIFY(approval(false)); QVERIFY(!dashboard->isVisible());
        QVERIFY(QMetaObject::invokeMethod(engine, "authenticationChanged", Q_ARG(bool, false)));
        QVERIFY(dashboard->isVisible());
        controller.command("hide");
        QVERIFY(approval(true)); QVERIFY(approval(false)); QVERIFY(!dashboard->isVisible());
        controller.command("show"); QVERIFY(approval(true));
        controller.command("hide"); QVERIFY(approval(false)); QVERIFY(!dashboard->isVisible());
    }
    void noticesStayOnDashboard() {
        Dashboard dashboard(caps); dashboard.show();
        dashboard.notice("Capture diagnostic");
        auto *notice = dashboard.findChild<QLabel *>("notice"); QVERIFY(notice);
        QVERIFY(notice->isVisible());
        dashboard.findChild<QPushButton *>("openSettings")->click();
        QVERIFY(!notice->isVisible());
        dashboard.notice("Another diagnostic");
        QVERIFY(!notice->isVisible());
        dashboard.home(); QVERIFY(notice->isVisible());
        QCOMPARE(notice->text(), QString("Another diagnostic"));
        dashboard.findChild<QPushButton *>("openOverlays")->click();
        QVERIFY(!notice->isVisible());
        dashboard.home(); dashboard.notice(""); QVERIFY(!notice->isVisible());
    }
    void disabledCheckboxKeepsCheckedFill() {
        QWidget host; applyTheme(&host, "violet");
        QCheckBox check("Enabled option", &host);
        host.resize(240, 60); check.setGeometry(10, 10, 220, 40);
        host.show(); check.setEnabled(false);
        check.setChecked(false); QCoreApplication::processEvents();
        const auto unchecked = check.grab().toImage();
        check.setChecked(true); QCoreApplication::processEvents();
        const auto checked = check.grab().toImage();
        int changed = 0;
        for (int y = 0; y < checked.height(); ++y)
            for (int x = 0; x < 18 * check.devicePixelRatioF(); ++x)
                if (checked.pixelColor(x, y) != unchecked.pixelColor(x, y)) ++changed;
        QVERIFY2(changed > 100, "Disabled checked boxes must keep a distinct filled background");
    }
    void startupOptionsAreIndependentAndPersisted() {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.ini");
        Settings settings(path);
        QVERIFY(!settings.launchAtLogin());
        QVERIFY(!settings.startReplayOnLaunch());
        SettingsPage page(caps);
        GsrOptions options; options.source = "DP-1"; options.outputDirectory = directory.path();
        page.edit(options, false, false, keys);
        auto *login = page.findChild<QCheckBox *>("launchAtLogin");
        auto *replay = page.findChild<QCheckBox *>("startReplayOnLaunch");
        QVERIFY(login); QVERIFY(replay);
        for (bool launch : {false, true}) for (bool start : {false, true}) {
            page.edit(options, false, false, keys, "violet", "top-right", launch, start);
            QCOMPARE(login->isChecked(), launch);
            QCOMPARE(replay->isChecked(), start);
            QVERIFY(replay->isEnabled());
            QString error;
            QVERIFY(settings.saveStartup(page.launchAtLogin(), page.startReplayOnLaunch(), &error));
            QCOMPARE(Settings(path).launchAtLogin(), launch);
            QCOMPARE(Settings(path).startReplayOnLaunch(), start);
        }
    }
    void inputOverlayDefaultsToOffAndPersistsSanitized() {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.ini");
        Settings settings(path);
        QCOMPARE(settings.inputOverlay().layout, QString("none"));
        QVERIFY(!settings.inputOverlay().enabled());
        QVERIFY(!settings.inputOverlay().sidecar);
        InputOverlaySettings value; value.layout = "qwerty"; value.sidecar = true; value.corner = "top-right"; value.scale = 150; value.opacity = 60;
        settings.setInputOverlay(value);
        QVERIFY(Settings(path).inputOverlay() == value);
        InputOverlaySettings hostile; hostile.layout = "../../etc/passwd"; hostile.sidecar = true; hostile.corner = "middle"; hostile.scale = 9999; hostile.opacity = -5;
        settings.setInputOverlay(hostile);
        const auto cleaned = Settings(path).inputOverlay();
        QCOMPARE(cleaned.layout, QString("none"));
        QVERIFY(!cleaned.sidecar); // nothing to record without a layout
        QCOMPARE(cleaned.corner, QString("bottom-left"));
        QCOMPARE(cleaned.scale, 200); QCOMPARE(cleaned.opacity, 20);
    }
    void inputOverlayPreviewPaintsLayoutOrEmptyState() {
        InputOverlayPreview preview; applyTheme(&preview, "violet");
        preview.resize(340, 190);
        const QImage empty = preview.grab().toImage();
        preview.setLayoutId("qwerty");
        QCOMPARE(preview.layoutId(), QString("qwerty")); QVERIFY(preview.hasLayout());
        const QImage neutral = preview.grab().toImage();
        QVERIFY(neutral != empty);
        Input::InputState held; held.apply({"key.w", true, 0}); preview.setLiveState(held);
        QVERIFY(preview.grab().toImage() != neutral);                        // only real state draws pressed keys
        preview.clearLiveState();
        QCOMPARE(preview.grab().toImage(), neutral);                         // no invented presses
        preview.setLayoutId("unknown-layout");
        QVERIFY(!preview.hasLayout());
    }
    void inputOverlayLayerFollowsPlaybackAndStaysOutOfTheWay() {
        QTemporaryDir directory;
        const QString clip = directory.filePath("clip.mp4");
        QWidget video; video.resize(800, 450);
        InputOverlayLayer layer(&video); applyTheme(&video, "violet");
        QVERIFY(!layer.load(clip));                       // no sidecar: nothing to draw
        QVERIFY(!layer.hasTimeline() && !layer.isVisible());
        QFile file(Input::InputTimeline::sidecarPath(clip));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"schema\":1,\"layout\":\"qwerty\",\"durationMs\":4000,\"events\":[[1000,\"key.w\",1],[2000,\"key.w\",0]]}");
        file.close();
        QVERIFY(layer.load(clip) && layer.hasTimeline());
        InputOverlaySettings settings; settings.layout = "qwerty"; settings.corner = "bottom-right"; settings.scale = 100; settings.opacity = 100;
        layer.setSettings(settings);
        layer.place(QRect(0, 0, 800, 450));
        layer.show();
        QVERIFY(layer.testAttribute(Qt::WA_TransparentForMouseEvents)); // never steals clicks from the player controls
        QVERIFY(layer.geometry().right() < 800 && layer.geometry().bottom() < 450);
        QVERIFY(layer.geometry().center().x() > 400 && layer.geometry().center().y() > 225); // bottom-right
        QVERIFY(layer.width() > 200 && layer.width() <= 800 * 0.31);
        layer.setPositionMs(0);
        const QImage idle = layer.grab().toImage();
        layer.setPositionMs(1500);
        const QImage held = layer.grab().toImage();
        QVERIFY(idle != held);                            // W is down at 1.5 s
        layer.setPositionMs(2500);
        QCOMPARE(layer.grab().toImage(), idle);           // released again, also after a forward seek
        layer.setPositionMs(1500);
        QCOMPARE(layer.grab().toImage(), held);           // and when seeking backwards
        settings.corner = "top-left"; layer.setSettings(settings); layer.place(QRect(0, 0, 800, 450));
        QVERIFY(layer.geometry().center().x() < 400 && layer.geometry().center().y() < 225);
        layer.clear();
        QVERIFY(!layer.hasTimeline() && !layer.isVisible());
    }
    void peripheralOverlayLivesInCaptureOverlaysNotGeneralSettings() {
        SettingsPage page(caps); applyTheme(&page, "violet");
        QVERIFY(!page.findChild<QComboBox *>("inputOverlayLayout")); // not in general Settings
        QVERIFY(!page.findChild<QCheckBox *>("inputOverlaySidecar"));
        OverlayEditor editor; applyTheme(&editor, "violet");
        auto *layout = editor.findChild<QComboBox *>("inputOverlayLayout"); QVERIFY(layout);
        QVERIFY(editor.findChild<QCheckBox *>("inputOverlaySidecar"));
        QCOMPARE(layout->count(), 6); // None + the five reference layouts
        QCOMPARE(layout->itemData(0).toString(), QString("none"));
    }
    void inputOverlaySettingsRoundTripThroughTheEditor() {
        OverlayEditor editor; applyTheme(&editor, "violet");
        auto *layout = editor.findChild<QComboBox *>("inputOverlayLayout"); QVERIFY(layout);
        auto *sidecar = editor.findChild<QCheckBox *>("inputOverlaySidecar"); QVERIFY(sidecar);
        auto *notice = editor.findChild<QLabel *>("inputOverlayNotice"); QVERIFY(notice);
        QSignalSpy changed(&editor, &OverlayEditor::inputOverlayChanged);
        editor.setInputOverlay({});
        QVERIFY(!sidecar->isEnabled()); // nothing is read while None is selected
        QVERIFY(notice->text().startsWith("Off."));
        QCOMPARE(changed.size(), 0);    // loading values is not a user edit
        InputOverlaySettings value; value.layout = "azerty"; value.corner = "bottom-right"; value.scale = 80; value.opacity = 70; value.sidecar = true;
        editor.setInputOverlay(value);
        QCOMPARE(changed.size(), 0);
        QVERIFY(sidecar->isEnabled() && sidecar->isChecked());
        layout->setCurrentIndex(layout->findData("qwerty"));   // a user edit emits the new value
        QCOMPARE(changed.size(), 1);
        const auto edited = changed.last().first().value<InputOverlaySettings>();
        QCOMPARE(edited.layout, QString("qwerty")); QCOMPARE(edited.scale, 80); QVERIFY(edited.sidecar);
        layout->setCurrentIndex(0);                            // back to None switches the sidecar off with it
        QVERIFY(!sidecar->isChecked());
        QVERIFY(!changed.last().first().value<InputOverlaySettings>().sidecar);
    }
    void fullLayoutWarnsAboutTypedTextAndOthersDoNot() {
        OverlayEditor editor; applyTheme(&editor, "violet");
        auto *notice = editor.findChild<QLabel *>("inputOverlayNotice"); QVERIFY(notice);
        InputOverlaySettings value; value.layout = "qwerty-full"; editor.setInputOverlay(value);
        QVERIFY(notice->text().contains("reconstruct typed text"));
        value.layout = "qwerty"; editor.setInputOverlay(value);
        QVERIFY(!notice->text().contains("reconstruct"));
        QVERIFY(notice->text().contains("never text"));
    }
    void inputIsOnlyRequestedWhileTheEditorShowsAnOverlay() {
        OverlayEditor editor; applyTheme(&editor, "violet"); editor.resize(1000, 700);
        QSignalSpy wanted(&editor, &OverlayEditor::inputPreviewWanted);
        editor.show(); QVERIFY(QTest::qWaitForWindowExposed(&editor));
        QVERIFY(wanted.isEmpty() || wanted.last().first().toString().isEmpty()); // None: nothing is read
        auto *layout = editor.findChild<QComboBox *>("inputOverlayLayout"); QVERIFY(layout);
        layout->setCurrentIndex(layout->findData("dualsense"));
        QCOMPARE(wanted.last().first().toString(), QString("dualsense"));
        layout->setCurrentIndex(0);
        QVERIFY(wanted.last().first().toString().isEmpty());
        layout->setCurrentIndex(layout->findData("qwerty"));
        QCOMPARE(wanted.last().first().toString(), QString("qwerty"));
        editor.hide();                                                       // leaving the editor stops reading
        QVERIFY(wanted.last().first().toString().isEmpty());
        editor.show(); QVERIFY(QTest::qWaitForWindowExposed(&editor));
        QCOMPARE(wanted.last().first().toString(), QString("qwerty"));
        auto *setup = editor.findChild<QLabel *>("inputOverlaySetup"); QVERIFY(setup);
        editor.setInputStatus(Input::InputStatus::PermissionDenied, "denied");
        QVERIFY(!setup->isHidden());
        QVERIFY(setup->text().contains("never changes permissions"));
        QVERIFY(!setup->text().contains("usermod"));                         // never tells users to join the input group
        editor.setInputStatus(Input::InputStatus::Active, {});
        QVERIFY(setup->isHidden());
        editor.setInputLive(Input::InputState{});
    }
    void captureCanvasShowsWherePeripheralOverlayWillSit() {
        OverlayCanvas canvas; applyTheme(&canvas, "violet"); canvas.resize(640, 360);
        canvas.setCaptureVisible(false);
        const QImage before = canvas.grab().toImage();
        InputOverlaySettings value; value.layout = "qwerty"; value.corner = "bottom-right";
        canvas.setInputOverlay(value);
        const QImage after = canvas.grab().toImage();
        QVERIFY(after != before);
        bool leftHalfChanged = false, rightHalfChanged = false;
        for (int y = 180; y < 360; ++y) for (int x = 0; x < 640; ++x)
            if (after.pixel(x, y) != before.pixel(x, y)) (x < 320 ? leftHalfChanged : rightHalfChanged) = true;
        QVERIFY(rightHalfChanged && !leftHalfChanged);                       // bottom-right corner only
        canvas.setInputOverlay({});
        QCOMPARE(canvas.grab().toImage(), before);
    }
    void noticeScrollsLongMessagesWithinBoundedHeight() {
        Dashboard dashboard(caps); dashboard.resize(900, 700); dashboard.show();
        auto *box = dashboard.findChild<QScrollArea *>("noticeScroll"); auto *notice = dashboard.findChild<QLabel *>("notice");
        QVERIFY(box && notice);
        dashboard.notice("Saved clip.mp4");
        QCOMPARE(notice->property("tone").toString(), QString("info"));
        const int shortHeight = box->height();
        QString longText; for (int i = 0; i < 40; ++i) longText += QString("Capture diagnostic line %1: something went wrong with the device. ").arg(i);
        dashboard.notice(longText);
        QCOMPARE(notice->property("tone").toString(), QString("warning"));
        QVERIFY(box->isVisible());
        QVERIFY(box->height() >= shortHeight && box->height() <= 96);          // bounded, never grows indefinitely
        QVERIFY(box->verticalScrollBar()->maximum() > 0);                     // the rest is reachable by scrolling
        QCOMPARE(notice->text(), longText);                                   // nothing is truncated
        auto *settings = dashboard.findChild<QPushButton *>("openSettings"); QVERIFY(settings && settings->isVisible());
        QVERIFY(dashboard.rect().contains(box->mapTo(&dashboard, box->rect().bottomLeft())));
    }
    void builtInMouseLayoutsAreVerticallyCentredOnTheKeyboard() {
        for (const QString id : {"qwerty", "azerty", "arrows"}) {
            Input::Layout layout; QString error; QVERIFY(Input::Layout::builtin(id, &layout, &error));
            double kbTop = 1e9, kbBottom = -1e9, mouseTop = 1e9, mouseBottom = -1e9;
            for (const auto &e : layout.elements()) {
                const bool mouse = e.id.startsWith("mouse.") || e.shape == "mouseBody";
                (mouse ? mouseTop : kbTop) = mouse ? qMin(mouseTop, e.rect.top()) : qMin(kbTop, e.rect.top());
                (mouse ? mouseBottom : kbBottom) = mouse ? qMax(mouseBottom, e.rect.bottom()) : qMax(kbBottom, e.rect.bottom());
            }
            QVERIFY2(qAbs((kbTop + kbBottom) / 2 - (mouseTop + mouseBottom) / 2) < 0.01, qPrintable(id));
        }
    }
    void fullKeyboardKeepsMouseAndArrowsInOneCompactColumn() {
        Input::Layout layout; QString error; QVERIFY(Input::Layout::builtin("qwerty-full", &layout, &error));
        QRectF mouse, up, space; double keysRight = 0;
        for (const auto &e : layout.elements()) {
            if (e.shape == "mouseBody") mouse = e.rect; else if (e.id == "key.up") up = e.rect;
            else if (e.id == "key.space") space = e.rect; else if (e.id == "key.backspace") keysRight = e.rect.right();
        }
        QVERIFY(mouse.bottom() < up.top());                                   // mouse sits above the arrows, no overlap
        QVERIFY(mouse.left() > keysRight && up.left() > keysRight);           // both right of the main keys
        QVERIFY(layout.size().width() / layout.size().height() < 3.2);        // compact silhouette, not a long strip
        QVERIFY(qAbs(mouse.center().x() - up.center().x()) < 0.01);           // column is centred on itself
        QVERIFY(space.width() > 5);                                            // still a recognisable full keyboard
    }
    void fullKeyboardGetsMoreRoomThanGameplayLayouts() {
        const QRect video(0, 0, 1920, 1080);
        const QRect full = Input::overlayRect(video, {24.05, 6.775}, "bottom-left", 100, "full");
        const QRect game = Input::overlayRect(video, {11.05, 5.62}, "bottom-left", 100, "gameplay");
        QVERIFY(full.width() > game.width());
        QVERIFY(full.width() <= video.width() * 40 / 100);                     // still moderate
        QCOMPARE(Input::overlayRect(video, {24.05, 6.775}, "bottom-left", 50, "full").width(), full.width() / 2);
    }
    void consentCardIsInlineCompactAndSelectsDevicesAutomatically() {
        using Shadowless::Input::AccessDevice;
        AccessDevice kb; kb.name = "MCHOSE Jet75-II"; kb.vendor = "41e4"; kb.product = "211a"; kb.cls = AccessDevice::Keyboard;
        AccessDevice ms; ms.name = "Compx Wireless mouse 8k dongle"; ms.vendor = "373b"; ms.product = "111a"; ms.cls = AccessDevice::Mouse;
        AccessDevice ready; ready.name = "Already Readable"; ready.vendor = "046d"; ready.product = "c548"; ready.cls = AccessDevice::Mouse; ready.readable = true;
        AccessDevice hybrid; hybrid.name = "Acme Combo"; hybrid.vendor = "1234"; hybrid.product = "abcd"; hybrid.cls = AccessDevice::Keyboard;
        AccessDevice pointer = hybrid; pointer.cls = AccessDevice::Mouse; hybrid.members = {hybrid, pointer};
        hybrid.alternatives = {AccessDevice::Mouse};
        QWidget host; host.resize(420, 400);
        auto *layout = new QVBoxLayout(&host);
        auto *card = new InputAccessDialog({kb, ms, ready, hybrid}, &host);
        layout->addWidget(card); host.show();
        QVERIFY(!card->isWindow());                                   // an inline card can never open behind the layer-shell editor
        QVERIFY(card->sizeHint().height() < 320);                               // compact: sized to its content, not half the screen
        QCOMPARE(card->selected().size(), 3);                         // unreadable devices preselected, the readable one is left alone
        QCOMPARE(card->selected().at(0).triple(), QString("41e4:211a:keyboard"));
        QCOMPARE(card->selected().at(1).triple(), QString("373b:111a:mouse"));
        auto *kind = card->findChild<QComboBox *>("inputAccessKind");  // only the true hybrid asks a question
        QVERIFY(kind); kind->setCurrentIndex(1);
        QCOMPARE(card->selected().at(2).triple(), QString("1234:abcd:mouse"));
        for (auto *box : card->findChildren<QCheckBox *>("inputAccessDevice")) box->setChecked(false);
        QVERIFY(!card->findChild<QPushButton *>("inputAccessContinue")->isEnabled());   // nothing selected: nothing to authorize
        QSignalSpy no(card, &InputAccessDialog::rejected), yes(card, &InputAccessDialog::accepted);
        card->findChild<QPushButton *>("inputAccessCancel")->click();
        QCOMPARE(no.size(), 1); QCOMPARE(yes.size(), 0);
    }
    void inputAccessControlsFollowAvailabilityAndNeverBlockTheApp() {
        QTemporaryDir dir; QFile rule(dir.filePath("70.rules")); QVERIFY(rule.open(QIODevice::WriteOnly)); rule.close();
        InputOverlayPanel panel; panel.show();
        InputOverlaySettings value; value.layout = "qwerty"; panel.setValue(value);
        auto *tool = panel.findChild<Input::InputAccess *>(); QVERIFY(tool);
        auto *enable = panel.findChild<QPushButton *>("inputOverlayEnableAccess"); auto *remove = panel.findChild<QPushButton *>("inputOverlayRemoveAccess");
        auto *manual = panel.findChild<QLabel *>("inputOverlaySetup"); QVERIFY(enable && remove && manual);
        tool->configureForTesting({}, {}, dir.filePath("absent"));                 // no pkexec or helper: only the manual steps remain
        panel.setInputStatus(Input::InputStatus::PermissionDenied, {});
        QVERIFY(enable->isHidden()); QVERIFY(remove->isHidden()); QVERIFY(!manual->isHidden());
        tool->configureForTesting("/bin/sh", "/bin/sh", rule.fileName());            // automatic setup available, rule already installed
        panel.setInputStatus(Input::InputStatus::PermissionDenied, {});
        QVERIFY(!enable->isHidden()); QVERIFY(manual->isHidden()); QVERIFY(!remove->isHidden());
        panel.setInputStatus(Input::InputStatus::Active, {});                        // already readable: LIVE, nothing is asked
        QVERIFY(enable->isHidden());
        QCOMPARE(panel.statusBadge(), QString("LIVE"));
        value.layout = "none"; panel.setValue(value);                                // overlay off: no setup controls at all
        QVERIFY(enable->isHidden() && remove->isHidden());
    }
    void overlayRectHonoursCornerMarginsAndShrinksGamepads() {
        const QRect video(0, 0, 1920, 1080);
        const QRect bl = Input::overlayRect(video, {11.05, 5.62}, "bottom-left", 100);
        QCOMPARE(bl.left(), 14); QCOMPARE(video.bottom() - bl.bottom(), 14);
        const QRect br = Input::overlayRect(video, {11.05, 5.62}, "bottom-right", 100);
        QCOMPARE(video.right() - br.right(), 14); QCOMPARE(video.bottom() - br.bottom(), 14);
        const QRect pad = Input::overlayRect(video, {11.0, 7.6}, "bottom-left", 100, "pad");
        QVERIFY(pad.width() < bl.width());                                    // placeholder gamepad is smaller by default
        QCOMPARE(video.bottom() - pad.bottom(), 14);
    }
    void captureCanvasKeepsBottomMarginAndNeutralState() {
        OverlayCanvas canvas; applyTheme(&canvas, "violet"); canvas.resize(640, 360);
        canvas.setCaptureVisible(false);
        QSignalSpy status(&canvas, &OverlayCanvas::statusTextChanged);
        canvas.setCaptureVisible(true); canvas.setCaptureVisible(false);
        QVERIFY(!status.isEmpty());                                            // status travels as text, not painted in the canvas
        QVERIFY(canvas.statusText().contains("not the recorded output"));
        const QImage before = canvas.grab().toImage();
        InputOverlaySettings value; value.layout = "qwerty"; value.corner = "bottom-left";
        canvas.setInputOverlay(value);
        const QImage neutral = canvas.grab().toImage();
        int lowest = -1;
        for (int y = 359; y >= 0 && lowest < 0; --y) for (int x = 0; x < 640; ++x) if (neutral.pixel(x, y) != before.pixel(x, y)) { lowest = y; break; }
        QVERIFY(lowest >= 0);
        const QRectF area = QRectF(0, 0, 640, 360);
        QVERIFY(area.bottom() - lowest <= 6 + 14 + 2);                         // only canvas margin + corner margin, no reserved strip
        Input::InputState held; held.apply({"key.w", true, 0}); canvas.setInputLive(held);
        QVERIFY(canvas.grab().toImage() != neutral);
        canvas.setInputOverlay(value);                                         // layout/settings changes reset to neutral
        QCOMPARE(canvas.grab().toImage(), neutral);
    }
    void peripheralStatusBadgeNeverClaimsLiveWithoutSource() {
        InputOverlayPanel panel; panel.show();
        QCOMPARE(panel.statusBadge(), QString("OFF"));
        InputOverlaySettings value; value.layout = "dualsense"; panel.setValue(value);
        auto *badge = panel.findChild<QLabel *>("inputOverlayStatus"); QVERIFY(badge);
        panel.setInputStatus(Input::InputStatus::PermissionDenied, {}); QCOMPARE(badge->text(), QString("PERMISSION DENIED"));
        panel.setInputStatus(Input::InputStatus::NoDevices, {});        QCOMPARE(badge->text(), QString("DISCONNECTED"));
        panel.setInputStatus(Input::InputStatus::Error, "x");           QCOMPARE(badge->text(), QString("ERROR"));
        QVERIFY(badge->text() != "LIVE");
        panel.setInputStatus(Input::InputStatus::Active, {});           QCOMPARE(badge->text(), QString("LIVE"));
        QVERIFY(!panel.findChild<QLabel *>("inputOverlayPlaceholder")->isHidden());
    }
    void failedStartupSavePreservesCurrentValues() {
        QTemporaryDir directory;
        QFile blocked(directory.filePath("not-a-directory"));
        QVERIFY(blocked.open(QIODevice::WriteOnly)); blocked.write("keep"); blocked.close();
        Settings settings(blocked.fileName() + "/settings.ini");
        QString error;
        QVERIFY(!settings.saveStartup(true, true, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!settings.launchAtLogin());
        QVERIFY(!settings.startReplayOnLaunch());
    }
    void libraryActionsFitNarrowPanel() {
        QTemporaryDir directory;
        for (int i = 0; i < 10; ++i) {
            QFile clip(directory.filePath(QString("Replay_2026-11-15_01-37-%1.mp4").arg(i)));
            QVERIFY(clip.open(QIODevice::WriteOnly)); clip.write("fixture");
        }
        Library library; applyTheme(&library, "mint");
        library.setFixedSize(400, 550); library.show(); library.refresh(directory.path());
        QTRY_COMPARE(library.findChildren<QPushButton *>("deleteClip").size(), 10);
        QTest::qWait(100);
        auto *scroll = library.findChild<QScrollArea *>(); QVERIFY(scroll);
        for (auto *action : library.findChildren<QPushButton *>("deleteClip")) {
            QVERIFY(action->mapTo(scroll->viewport(), QPoint(action->width(), 0)).x() <= scroll->viewport()->width());
        }
    }
    void libraryOpensContainingGameFolder() {
        QTemporaryDir directory;
        QVERIFY(QDir(directory.path()).mkpath("A game"));
        QFile file(directory.filePath("A game/replay.mp4"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip"); file.close();
        Library library; library.show(); library.refresh(directory.path());
        QTRY_VERIFY(library.findChild<QPushButton *>("openClipFolder"));
        auto *folder = library.findChild<QPushButton *>("openClipFolder");
        auto *remove = library.findChild<QPushButton *>("deleteClip");
        QVERIFY(remove); QVERIFY(!folder->icon().isNull());
        QCOMPARE(folder->parentWidget()->layout()->indexOf(folder) + 1,
                 folder->parentWidget()->layout()->indexOf(remove));
        QDesktopServices::setUrlHandler("file", this, "captureOpenedUrl");
        const auto cleanup = qScopeGuard([] { QDesktopServices::unsetUrlHandler("file"); });
        folder->click();
        QCOMPARE(m_openedUrl.toLocalFile(), directory.filePath("A game"));
    }
    void libraryKeepsNewestTwentyInEveryCollection() {
        QTemporaryDir directory;
        const auto origin = QDateTime::fromSecsSinceEpoch(1700000000);
        for (const auto &collection : {QString("Older game"), QString("Newer game")}) {
            QVERIFY(QDir(directory.path()).mkpath(collection));
            for (int i = 0; i < 30; ++i) {
                QFile file(directory.filePath(collection + QString("/%1.mp4").arg(i)));
                QVERIFY(file.open(QIODevice::WriteOnly));
                file.write("clip");
                QVERIFY(file.flush());
                QVERIFY(file.setFileTime(origin.addSecs(i + (collection == "Newer game" ? 100 : 0)), QFileDevice::FileModificationTime));
            }
        }
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty());
        QStringList newest;
        for (int i = 29; i >= 10; --i) newest.append(directory.filePath(QString("Newer game/%1.mp4").arg(i)));
        QTRY_COMPARE(clips.last().at(0).toStringList(), newest);
        auto *filter = library.findChild<QComboBox *>("collectionFilter"); QVERIFY(filter);
        filter->setCurrentIndex(filter->findText("Older game"));
        QStringList older;
        for (int i = 29; i >= 10; --i) older.append(directory.filePath(QString("Older game/%1.mp4").arg(i)));
        QCOMPARE(clips.last().at(0).toStringList(), older);
    }
    void libraryFindsAndFiltersGameCollections() {
        QTemporaryDir directory;
        QVERIFY(QDir(directory.path()).mkpath("Rocket League"));
        for (const auto &name : {QString("desktop.mp4"), QString("Rocket League/goal.mp4")}) {
            QFile file(directory.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip");
        }
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty());
        QTRY_COMPARE(clips.last().at(0).toStringList().size(), 2);
        auto *filter = library.findChild<QComboBox *>("collectionFilter"); QVERIFY(filter);
        const int game = filter->findText("Rocket League"); QVERIFY(game >= 0);
        filter->setCurrentIndex(game);
        QTRY_COMPARE(clips.last().at(0).toStringList(), QStringList{directory.filePath("Rocket League/goal.mp4")});
        for (int i = 0; i < 25; ++i) {
            QFile file(directory.filePath(QString("desktop-%1.mp4").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly)); file.write("newer clip");
        }
        QSignalSpy scanned(&library, &Library::storageChanged);
        library.refresh(directory.path()); QTRY_VERIFY(!scanned.isEmpty());
        QCOMPARE(filter->currentText(), QString("Rocket League"));
        QCOMPARE(clips.last().at(0).toStringList(), QStringList{directory.filePath("Rocket League/goal.mp4")});
    }
    void libraryDeletionRequiresConfirmationAndPreservesOtherClips() {
        QTemporaryDir directory;
        const auto path = directory.filePath("delete-me.mp4");
        const auto keep = directory.filePath("keep.mp4");
        for (const auto &name : {path, keep}) {
            QFile file(name); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip");
        }
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        QSignalSpy opened(&library, &Library::clipRequested);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty() && clips.last().at(0).toStringList().size() == 2);
        QPushButton *remove = nullptr;
        for (auto *candidate : library.findChildren<QPushButton *>("deleteClip"))
            if (candidate->accessibleName().contains("delete-me.mp4")) remove = candidate;
        QVERIFY(remove);
        remove->click();
        QVERIFY(QFileInfo::exists(path));
        auto *cancel = library.findChild<QPushButton *>("cancelDeleteClip"); QVERIFY(cancel);
        cancel->click(); QVERIFY(QFileInfo::exists(path));
        remove->click();
        auto *confirm = library.findChild<QPushButton *>("confirmDeleteClip"); QVERIFY(confirm);
        confirm->click();
        QTRY_VERIFY(!QFileInfo::exists(path));
        QTRY_COMPARE(clips.last().at(0).toStringList(), QStringList{keep});
        QVERIFY(QFileInfo::exists(keep));
        QVERIFY(opened.isEmpty());
    }
    void libraryDeletionReportsFailure() {
        QTemporaryDir directory;
        const auto path = directory.filePath("missing.mp4");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip"); file.close();
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty() && clips.last().at(0).toStringList().size() == 1);
        auto *remove = library.findChild<QPushButton *>("deleteClip"); QVERIFY(remove); remove->click();
        QVERIFY(QFile::remove(path));
        library.findChild<QPushButton *>("confirmDeleteClip")->click();
        auto *error = library.findChild<QLabel *>("deleteClipError"); QVERIFY(error);
        QVERIFY(error->isVisible()); QVERIFY(!error->text().isEmpty());
    }
    void settingsOffersOptionalGameOrganization() {
        SettingsPage page(caps); GsrOptions options; options.source = "DP-1"; options.outputDirectory = "/tmp/clips";
        page.edit(options, false, false, keys);
        auto *check = page.findChild<QCheckBox *>("organizeByGame"); QVERIFY(check);
        QVERIFY(!check->isChecked());
        check->setChecked(true);
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1);
        const auto changed = qvariant_cast<GsrOptions>(saved.at(0).at(0));
        QVERIFY(changed.organizeByGame);
        QTemporaryDir directory; QString error;
        Settings settings(directory.filePath("settings.ini"));
        QVERIFY(settings.save(changed, {}, false, &error));
        QVERIFY(settings.capture(caps).organizeByGame);
    }
    void clipRoutingPreservesFilesAndRejectsSymlinkFolders() {
        QTemporaryDir directory, outside; QString error;
        const auto path = directory.filePath("goal.mp4");
        auto create = [&] { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("new clip"); };
        create();
        QCOMPARE(routeClip(directory.path(), path, {}, &error), path);
        const auto first = routeClip(directory.path(), path, "Rocket League", &error);
        QVERIFY(error.isEmpty()); QCOMPARE(first, directory.filePath("Rocket League/goal.mp4"));
        create();
        const auto second = routeClip(directory.path(), path, "Rocket League", &error);
        QVERIFY(error.isEmpty()); QVERIFY(first != second); QVERIFY(QFileInfo::exists(first)); QVERIFY(QFileInfo::exists(second));
        create();
        QVERIFY(QFile::link(outside.path(), directory.filePath("Linked")));
        QCOMPARE(routeClip(directory.path(), path, "Linked", &error), path);
        QVERIFY(!error.isEmpty()); QVERIFY(QFileInfo::exists(path));
        const auto sanitized = routeClip(directory.path(), path, "../../escape", &error);
        QVERIFY(error.isEmpty()); QCOMPARE(QFileInfo(sanitized).absolutePath(), directory.filePath(".._.._escape"));
    }
    void gameIdentificationRejectsUnknownForegroundAndAmbiguousExecutables() {
        QJsonObject game{{"mapped", true}, {"initialClass", "rocketleague.exe"}, {"focusHistoryID", 0}};
        QCOMPARE(gameForClients({game}), QString("Rocket League"));
        game["initialClass"] = "game.exe";
        QVERIFY(gameForClients({game}).isEmpty());
        game["initialClass"] = "rocketleague.exe"; game["focusHistoryID"] = 1;
        QJsonObject browser{{"mapped", true}, {"initialClass", "firefox"}, {"focusHistoryID", 0}};
        QVERIFY(gameForClients({game, browser}).isEmpty());
        game["hidden"] = true; QVERIFY(gameForClients({game}).isEmpty());
        game["hidden"] = false; game["focusHistoryID"] = -1;
        QVERIFY(gameForClients({game}).isEmpty());
    }
    void clipNotificationReplacesItsTimeout() {
        ClipNotification toast;
        toast.present("Saving", {}, 20);
        toast.present("Saved", "goal.mp4", 120);
        QTest::qWait(45); QVERIFY(toast.isVisible());
        QTRY_VERIFY(!toast.isVisible());
        toast.present("Saved", "goal.mp4"); toast.dismiss(); QVERIFY(!toast.isVisible());
    }
    void clipNotificationPaintsCardAndStatusIcon() {
        ClipNotification toast; toast.present("Clip saved", "goal.mp4");
        const auto image = toast.grab().toImage();
        const auto card = image.pixelColor(180, 75);
        QVERIFY(card.alpha() > 200 && card.red() < 50 && card.green() < 50);
        int accentPixels = 0;
        for (int y = 20; y < 65; ++y) for (int x = 12; x < 60; ++x) {
            const auto pixel = image.pixelColor(x, y);
            if (pixel.blue() > 200 && pixel.red() > 140 && pixel.blue() > pixel.green() * 1.3) ++accentPixels;
        }
        QVERIFY(accentPixels > 50);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(image.save(output + ".notification.png"));
    }
    void clipNotificationCountsDownAndRestarts() {
        ClipNotification toast;
        toast.setAccent(QColor("#a855f7"));
        const auto barPixels = [&toast] {
            const auto image = toast.grab().toImage();
            int count = 0;
            for (int x = 15; x < toast.width() - 15; ++x) {
                const auto pixel = image.pixelColor(x, toast.height() - 2);
                if (pixel.red() > 100 && pixel.blue() > 150) ++count;
            }
            return count;
        };
        toast.present("Clip saved", "first.mp4", 1000);
        const int full = barPixels();
        QVERIFY(full > 300);
        QTest::qWait(400);
        const int remaining = barPixels();
        QVERIFY(remaining < full * .8);
        QVERIFY(remaining > 0);
        toast.present("Clip saved", "second.mp4", 1000);
        QVERIFY(barPixels() > remaining + 50);
        toast.dismiss();
        QVERIFY(!toast.isVisible());
        for (auto *animation : toast.findChildren<QVariantAnimation *>())
            QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    }
    void controllerCreatesPassiveClipNotification() {
        Controller controller(caps, {}, true);
        QWidget *toast = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) if (widget->objectName() == "clipNotification") toast = widget;
        QVERIFY(toast);
        QVERIFY(toast->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
        QVERIFY(toast->windowFlags().testFlag(Qt::WindowTransparentForInput));
        QVERIFY(toast->testAttribute(Qt::WA_ShowWithoutActivating));
        QVERIFY(toast->testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(!toast->isVisible());
        QFile sound(":/shadowless/replay-saved.wav"); QVERIFY(sound.open(QIODevice::ReadOnly));
        QCOMPARE(sound.read(4), QByteArray("RIFF"));
    }
    void dashboardModeCardsFitTheirContainer() {
        Dashboard dashboard(caps);
        dashboard.show();
        const auto tiles = dashboard.findChildren<QPushButton *>("captureMode");
        QCOMPARE(tiles.size(), 2);
        for (const auto size : {QSize(1280, 720), QSize(1920, 1080), QSize(740, 720)}) {
            dashboard.resize(size);
            QCoreApplication::processEvents();
            for (auto *tile : tiles) {
                QVERIFY(tile->height() >= 120 && tile->height() <= 140);
                QVERIFY(tile->parentWidget()->rect().contains(tile->geometry()));
                auto *overlay = dashboard.findChild<QPushButton *>("overlayToggle");
                QVERIFY(overlay->mapTo(&dashboard, QPoint()).y() < tile->mapTo(&dashboard, QPoint()).y());
            }
        }
    }
    void initTestCase() { qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/dev/null"); }
    void offersFourIndicatorCorners() {
        SettingsPage page(caps);
        auto *position = page.findChild<QComboBox *>("indicatorPosition");
        QVERIFY(position);
        QCOMPARE(position->count(), 4);
        QCOMPARE(position->currentData().toString(), "top-right");
        QTemporaryDir directory;
        const auto settingsPath = directory.filePath("settings.ini");
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = directory.path();
        page.edit(options, false, false, keys);
        for (int i = 0; i < 4; ++i) {
            position->setCurrentIndex(i);
            QSignalSpy saved(&page, &SettingsPage::saved);
            button(&page, "Save settings")->click();
            QCOMPARE(saved.size(), 1);
            const auto corner = saved.first().at(4).toString();
            QCOMPARE(corner, position->currentData().toString());
            Settings settings(settingsPath);
            QString error;
            QVERIFY(settings.save(GsrOptions{}, {}, true, &error, keys, "mint", corner));
            QCOMPARE(Settings(settingsPath).indicatorPosition(), corner);
            page.edit(options, true, false, keys, "mint", corner);
            QCOMPARE(position->currentData().toString(), corner);
        }
    }
    void previewsCustomOverlaysOnDashboard() {
        Dashboard dashboard(caps);
        auto *preview = dashboard.findChild<OverlayCanvas *>("overlayPreview");
        QVERIFY(preview);
        QTemporaryDir directory;
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath("custom.png");
        QVERIFY(image.save(path));
        OverlayLayer layer;
        layer.path = path;
        OverlayScene scene;
        scene.enabled = true;
        scene.layers.append(layer);
        dashboard.updateState(GsrOptions{}, scene, false, false, false, false);
        dashboard.show();
        QCoreApplication::processEvents();
        QVERIFY(preview->isVisible());
        const auto rendered = preview->grab().toImage();
        int redPixels = 0;
        for (int y = 0; y < rendered.height(); ++y)
            for (int x = 0; x < rendered.width(); ++x) {
                const auto color = rendered.pixelColor(x, y);
                if (color.red() > 200 && color.green() < 40) ++redPixels;
            }
        QVERIFY(redPixels > 100);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output + ".preview.png"));
        dashboard.updateState(GsrOptions{}, {}, false, false, false, false);
        QVERIFY(!preview->isVisible());
    }
    void includesAnEmbeddedPlayer() {
        Dashboard dashboard(caps);
        auto *player = dashboard.findChild<ClipPlayer *>("clipPlayer");
        QVERIFY(player);
        QTemporaryDir directory;
        const auto path = directory.filePath("sample.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-t", "3", "-c:v", "mpeg4", path}), 0);
        dashboard.show();
        QSignalSpy hidden(&dashboard, &Dashboard::hideRequested);
        auto *library = dashboard.findChild<Library *>();
        library->clipRequested(path);
        QVERIFY(player->isVisible());
        QCOMPARE(hidden.size(), 0);
        auto *media = player->findChild<MpvPlayer *>();
        QVERIFY(media);
        QSignalSpy frames(media, &MpvPlayer::frameRendered);
        QSignalSpy errors(media, &MpvPlayer::error);
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() > 0, 5000);
        QTRY_VERIFY(media->duration() > 0);
        player->findChild<QPushButton *>("clipPlayPause")->click();
        QTRY_VERIFY(media->paused());
        auto *seek = player->findChild<QSlider *>("clipSeek");
        QVERIFY(seek);
        seek->setValue(0);
        seek->setValue(1000);
        QTRY_VERIFY(media->position() >= 900);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output + ".player.png"));
        dashboard.escape();
        QVERIFY(media->paused());
        QVERIFY(media->source().isEmpty());
        QCOMPARE(hidden.size(), 0);
        QCOMPARE(errors.size(), 0);
    }
    void playerOffersEditingAndFullscreen() {
        ClipPlayer player;
        QVERIFY2(player.findChild<QPushButton *>("clipFullscreen"), "Fullscreen transport is missing");
        QVERIFY(player.findChild<QPushButton *>("clipNext"));
        QVERIFY(player.findChild<QPushButton *>("clipPrevious"));
        QVERIFY(player.findChild<QComboBox *>("clipSpeed"));
        QVERIFY(player.findChild<QPushButton *>("clipTrim"));
    }
    void editorLayoutKeepsTimelineAndExportVisible() {
        QTemporaryDir directory;
        const auto path=directory.filePath("layout.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","testsrc2=s=320x180:r=15","-t","2","-c:v","mpeg4",path}),0);
        Dashboard dashboard(caps); dashboard.resize(1280,800); dashboard.show();
        auto *player=dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media=player->findChild<MpvPlayer *>();
        QSignalSpy frames(media,&MpvPlayer::frameRendered);
        QTRY_VERIFY(media->loaded()); QTRY_VERIFY(!frames.isEmpty()); media->pause(true);
        player->findChild<QPushButton *>("clipTrim")->click();
        auto *exportButton=player->findChild<QPushButton *>("clipExport");
        QTRY_VERIFY_WITH_TIMEOUT(exportButton->isEnabled(),15000);
        auto *timeline=player->findChild<QWidget *>("clipTimeline");
        const auto output=qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if(!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output+".editor.png"));
        auto *audio=player->findChild<QScrollArea *>("clipAudioTracks");
        auto *audioHeading=player->findChild<QLabel *>("clipAudioHeading"); QVERIFY(audioHeading);
        // Reserve the same vertical space as four audio rows without opening an audio device.
        const QSignalBlocker block(media);
        audio->setFixedHeight(184); audio->show();
        audioHeading->setText("AUDIO TRACKS · 4 · Select tracks to keep"); audioHeading->show();
        QCOMPARE(audio->parentWidget(),timeline->parentWidget());
        for(const auto size:{QSize(1280,800),QSize(1024,768)}) {
            dashboard.resize(size); QCoreApplication::processEvents();
            QVERIFY(timeline->mapTo(player,QPoint()).y()>media->mapTo(player,media->rect().bottomLeft()).y());
            QVERIFY(exportButton->mapTo(player,QPoint()).y()>timeline->mapTo(player,timeline->rect().bottomLeft()).y());
            QVERIFY(timeline->mapTo(player,QPoint()).y()<audio->mapTo(player,QPoint()).y());
            QVERIFY2(player->rect().contains(QRect(exportButton->mapTo(player,QPoint()),exportButton->size())),
                     qPrintable(QString("size=%1x%2 export=%3,%4 %5x%6 audio y=%7 height=%8 player=%9x%10 error='%11' visible=%12").arg(size.width()).arg(size.height()).arg(exportButton->mapTo(player,QPoint()).x()).arg(exportButton->mapTo(player,QPoint()).y()).arg(exportButton->width()).arg(exportButton->height()).arg(audio->mapTo(player,QPoint()).y()).arg(audio->height()).arg(player->width()).arg(player->height()).arg(player->findChild<QLabel *>("clipError")?player->findChild<QLabel *>("clipError")->text():QString("-")).arg(player->findChild<QLabel *>("clipError")?player->findChild<QLabel *>("clipError")->isVisible():false)));
            QVERIFY(player->rect().contains(QRect(exportButton->mapTo(player,QPoint()),exportButton->size())));
        }
    }
    void fullscreenControlsKeepVideoGeometry() {
        QTemporaryDir directory; const auto path=directory.filePath("fullscreen.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","12","-c:v","mpeg4",path}),0);
        Dashboard dashboard(caps); dashboard.resize(1600,1000); dashboard.show();
        auto *player=dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media=player->findChild<MpvPlayer *>();
        QTRY_VERIFY(media->loaded());
        player->setFullscreen(true);
        auto *play=player->findChild<QPushButton *>("clipPlayPause");
        QTest::mouseMove(media,QPoint(50,50));
        QCoreApplication::processEvents();
        const auto videoGeometry=media->geometry();
        const auto panelGeometry=dashboard.findChild<QWidget *>("shadowlessPanel")->geometry();
        QCOMPARE(media->mapTo(player,QPoint(0,0)),QPoint(0,0));
        QCOMPARE(media->size(),player->size());
        const auto playCenter=play->mapTo(player,play->rect().center());
        QVERIFY(media->rect().contains(media->mapFrom(player,playCenter)));
        QCOMPARE(player->childAt(playCenter),play);
        QTRY_VERIFY_WITH_TIMEOUT(!play->isVisible(),5000);
        QCoreApplication::processEvents();
        QCOMPARE(media->geometry(),videoGeometry);
        QTest::mouseMove(media,QPoint(80,80));
        QTRY_VERIFY(play->isVisible());
        QCOMPARE(media->geometry(),videoGeometry);
        QCOMPARE(dashboard.findChild<QWidget *>("shadowlessPanel")->geometry(),panelGeometry);
        dashboard.resize(1280,800); QCoreApplication::processEvents();
        QCOMPARE(media->size(),player->size());
        const auto resizedCenter=play->mapTo(player,play->rect().center());
        QCOMPARE(player->childAt(resizedCenter),play);
        player->setFullscreen(false); QCoreApplication::processEvents();
        QVERIFY(play->mapTo(player,QPoint(0,0)).y()>=media->mapTo(player,media->rect().bottomLeft()).y());
    }
    void spacePausesInsteadOfActivatingFocusedTransport() {
        QTemporaryDir directory; const auto path=directory.filePath("space.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","12","-c:v","mpeg4",path}),0);
        ClipPlayer player; player.resize(1000,750); player.show(); player.open(path);
        auto *media=player.findChild<MpvPlayer *>();
        auto *edit=player.findChild<QPushButton *>("clipTrim");
        QTRY_VERIFY(media->loaded()); QTRY_VERIFY(!media->paused());
        for(const auto &name:{"clipTrim","clipFullscreen","clipSeekForward","clipPlayPause"}) {
            auto *button=player.findChild<QPushButton *>(name); button->setFocus();
            QTest::keyClick(button,Qt::Key_Space); QTRY_VERIFY(media->paused());
            QVERIFY(!player.editing()); QVERIFY(!player.fullscreen());
            QTest::keyClick(button,Qt::Key_Space); QTRY_VERIFY(!media->paused());
            QVERIFY(!edit->isChecked());
        }
        edit->click(); QTRY_VERIFY(player.editing());
        QTRY_VERIFY(!player.findChild<MediaTools *>()->busy());
        edit->setFocus();
        const bool paused=media->paused();
        QTest::keyClick(edit,Qt::Key_Space); QTRY_COMPARE(media->paused(),!paused);
        QVERIFY(player.editing());
    }
    void editorExpandsAndResetsAfterDashboard() {
        QTemporaryDir directory; const auto path=directory.filePath("edit.mp4"), other=directory.filePath("other.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","4","-c:v","mpeg4",path}),0);
        QVERIFY(QFile::copy(path,other));
        Dashboard dashboard(caps); dashboard.resize(1600,1000); dashboard.show();
        auto *library=dashboard.findChild<Library *>(); auto *player=dashboard.findChild<ClipPlayer *>();
        auto *edit=player->findChild<QPushButton *>("clipTrim");
        auto *timeline=player->findChild<QWidget *>("clipTimeline");
        auto *panel=dashboard.findChild<QWidget *>("shadowlessPanel");
        for(const auto &clip:{path,path,other}) {
            library->clipRequested(clip);
            QVERIFY(!edit->isChecked()); QVERIFY(!timeline->isVisible());
            const auto normal=panel->geometry();
            edit->click();
            QTRY_VERIFY(panel->width()>dashboard.width()*0.9);
            QVERIFY(panel->height()>dashboard.height()*0.9);
            QVERIFY(panel->width()<dashboard.width()); QVERIFY(panel->height()<dashboard.height());
            player->setFullscreen(true); dashboard.escape();
            QVERIFY(panel->width()>dashboard.width()*0.9);
            edit->click(); QCOMPARE(panel->geometry(),normal);
            edit->click(); dashboard.home();
        }
    }
    void playerSeeksWhereClickedAndDragged() {
        QTemporaryDir directory;
        const auto path = directory.filePath("seek.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-t", "20", "-c:v", "mpeg4", path}), 0);
        ClipPlayer player; player.resize(1000, 650); player.show(); player.open(path);
        auto *media = player.findChild<MpvPlayer *>();
        auto *seek = player.findChild<QSlider *>("clipSeek");
        QTRY_VERIFY(media->duration() >= 19000);
        media->pause(true); QTRY_VERIFY(media->paused());
        QTest::mouseClick(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width()*3/4, seek->height()/2));
        QTRY_VERIFY2(qAbs(media->position()-15000) < 500, qPrintable(QString::number(media->position())));
        QTest::mousePress(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width()/4, seek->height()/2));
        QTest::mouseMove(seek, QPoint(seek->width()/2, seek->height()/2));
        QTest::mouseRelease(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width()/2, seek->height()/2));
        QTRY_VERIFY(qAbs(media->position()-10000) < 500);
        QTest::qWait(200); QVERIFY(qAbs(media->position()-10000) < 500); QVERIFY(media->paused());
        auto *mute=player.findChild<QPushButton *>("clipMute");
        mute->setFocus(); QTest::keyClick(mute,Qt::Key_Space);
        QVERIFY(mute->isChecked()); QTest::qWait(100); QVERIFY(media->paused());
        media->pause(false); QTRY_VERIFY(!media->paused());
        QTest::mousePress(seek,Qt::LeftButton,Qt::NoModifier,QPoint(seek->width()/4,seek->height()/2));
        QTRY_VERIFY(media->paused());
        QTest::mouseRelease(seek,Qt::LeftButton,Qt::NoModifier,QPoint(seek->width()/4,seek->height()/2));
        QTRY_VERIFY(!media->paused()); QTRY_VERIFY(qAbs(media->position()-5000)<500);
    }
    void editorWaveformsStayDarkAndVisible() {
        QWidget root; QPalette light; light.setColor(QPalette::Base,Qt::white); root.setPalette(light);
        auto *layout=new QVBoxLayout(&root); auto *wave=new ClipWaveform; layout->addWidget(wave);
        root.resize(640,80); applyTheme(&root); root.show(); QCoreApplication::processEvents();
        for(const auto &accent:{"violet","#000000","#0000ff"}) for(bool audible:{false,true}) {
            applyTheme(&root,accent);
            wave->peaks.fill(audible?.2f:0.f,512);
            const auto image=wave->grab().toImage(); QVERIFY(!image.isNull());
            QVERIFY2(image.pixelColor(4,4).lightness()<100,"Audio waveforms must use a dark surface, including silent tracks");
            const auto center=image.pixelColor(image.width()/2,image.height()/2-5);
            QCOMPARE(center.lightness()>100,audible);
        }
        wave->setEnabled(false);
        QVERIFY(wave->grab().toImage().pixelColor(4,4).lightness()<100);
    }
    void audioSelectorsStayLeftWithAndWithoutWaveforms() {
        ClipAudioTrack row("Microphone"); applyTheme(&row); row.resize(800,40); row.show();
        for(bool editing:{false,true}) {
            row.waveform->setVisible(editing); QCoreApplication::processEvents();
            QVERIFY2(row.selector->x()<=8,"Track selection must stay at the left edge in preview and editing");
            if(editing) QVERIFY(row.waveform->x()>row.selector->geometry().right());
            row.selector->setFocus(); const bool checked=row.selector->isChecked();
            QTest::keyClick(row.selector,Qt::Key_Space); QCOMPARE(row.selector->isChecked(),!checked);
            QTest::mouseClick(row.selector,Qt::LeftButton,Qt::NoModifier,QPoint(180,16));
            QCOMPARE(row.selector->isChecked(),checked);
        }
    }
    void dynamicAudioGroupsSharePhysicalSelection() {
        ClipAudioTracks rows; applyTheme(&rows); rows.resize(640,500); rows.show();
        int changes=0; rows.selectionChanged=[&] { ++changes; };
        rows.setTracks({{4,"PC (other)"},{8,"App: Browser"},{12,"Game: Arena"},{16,"Microphone"},{20,"App (off): Music"}});
        QCOMPARE(rows.selectedIds(),QVector<int>({4,8,12,16}));
        QCOMPARE(rows.selectedOrdinals(),QVector<int>({0,1,2,3}));
        auto *more=rows.findChild<QPushButton *>("clipMoreTracks"); QVERIFY(more); QVERIFY(!more->isVisible()); // player: groups only
        rows.setEditing(true); QVERIFY(more->isVisible()); // More tracks exists only in the editor
        auto *app=rows.findChild<QCheckBox *>("clipTrack1"); QVERIFY(app); QVERIFY(!app->isVisible());
        const int before=changes; more->click(); QVERIFY(app->isVisible()); QCOMPARE(changes,before);
        QCOMPARE(rows.selectedIds(),QVector<int>({4,8,12,16}));
        auto *pc=rows.findChild<QCheckBox *>("clipGroupPC"); QVERIFY(pc);
        QCOMPARE(pc->checkState(),Qt::PartiallyChecked);
        int prompts=0; bool accept=false; QString lastGroup; rows.confirmEnableAll=[&](const QString &group) { ++prompts; lastGroup=group; return accept; };
        pc->click(); QCOMPARE(prompts,1); QCOMPARE(rows.selectedIds(),QVector<int>({4,8,12,16})); QCOMPARE(pc->checkState(),Qt::PartiallyChecked); // Cancel
        QCOMPARE(lastGroup,QString("PC"));
        accept=true; pc->click(); QCOMPARE(prompts,2); QCOMPARE(rows.selectedIds(),QVector<int>({4,8,12,16,20})); // Ok enables every PC track
        pc->click(); QCOMPARE(prompts,2); QCOMPARE(rows.selectedIds(),QVector<int>({12,16})); // fully checked: one click turns the group off
        pc->click(); QCOMPARE(prompts,2); QCOMPARE(rows.selectedIds(),QVector<int>({4,8,12,16,20}));
        app->setFocus(); QTest::keyClick(app,Qt::Key_Space); QVERIFY(!rows.selectedIds().contains(8));
        rows.setTracks({{4,"PC (other)"},{8,"App: Browser"},{12,"Game: Arena"},{16,"Microphone"},{20,"App (off): Music"},{24,"App: Late app"}});
        QVERIFY(!rows.selectedIds().contains(8)); QVERIFY(rows.selectedIds().contains(24));
        QCOMPARE(rows.selectedOrdinals(),QVector<int>({0,2,3,4,5}));
        const auto selected=rows.selectedIds(); more->click(); QCOMPARE(rows.selectedIds(),selected);
        rows.setTracks({{4,"PC (other)"},{8,"App: Renamed browser"},{12,"Game: Arena"},{16,"Microphone"},{20,"App (off): Music"},{24,"App: Late app"}});
        QCOMPARE(rows.selectedIds(),selected);
    }
    void dynamicAudioGroupsPreserveLegacyAndResetExpansion() {
        ClipAudioTracks rows; rows.resize(420,600); rows.show();
        rows.setTracks({{1,"PC"},{2,"Microphone"}});
        QVERIFY(!rows.findChild<QPushButton *>("clipMoreTracks")->isVisible());
        QCOMPARE(rows.selectedIds(),QVector<int>({1,2}));
        QVERIFY(!rows.findChild<QCheckBox *>("clipGroupPC")->isVisible());
        rows.setTracks({{1,"PC (other)"},{2,"Microphone"}});
        auto *game=rows.findChild<QCheckBox *>("clipGroupGame"); QVERIFY(game->isVisible()); QVERIFY(!game->isEnabled());
        auto *more=rows.findChild<QPushButton *>("clipMoreTracks"); more->click();
        rows.clear(); rows.setTracks({{1,"PC (other)"},{2,"App: New clip"}});
        QVERIFY(!more->isChecked()); QVERIFY(!rows.findChild<QCheckBox *>("clipTrack1")->isVisible());
        rows.findChild<QCheckBox *>("clipGroupPC")->setFocus();
        QTest::keyClick(rows.findChild<QCheckBox *>("clipGroupPC"),Qt::Key_Space);
        QVERIFY(rows.selectedIds().isEmpty());
    }
    void dynamicAudioOffDefaultsRespectLateMetadataAndUserSelection() {
        ClipAudioTracks rows; rows.resize(500,400); rows.show();
        rows.setTracks({{1,"Audio track 1"},{2,"Audio track 2"},{3,"Audio track 3"},{4,"Audio track 4"}});
        auto *first=rows.findChild<QCheckBox *>("clipTrack0"); QVERIFY(first); first->setChecked(false);
        rows.setTracks({{1,"App: Browser"},{2,"App (off): Music"},{3,"Game (off): Arena"},{4,"Microphone"}});
        QCOMPARE(rows.selectedIds(),QVector<int>({4}));
        QCOMPARE(rows.selectedOrdinals(),QVector<int>({3}));
        auto *pc=rows.findChild<QCheckBox *>("clipGroupPC"), *game=rows.findChild<QCheckBox *>("clipGroupGame");
        QVERIFY(pc->isEnabled()); QVERIFY(game->isEnabled());
        QCOMPARE(pc->checkState(),Qt::Unchecked); QCOMPARE(game->checkState(),Qt::Unchecked);
        game->click(); QCOMPARE(rows.selectedIds(),QVector<int>({3,4}));
        pc->click(); QCOMPARE(rows.selectedIds(),QVector<int>({1,2,3,4}));
        rows.setTracks({{1,"App: Browser"},{2,"App (off): Music"},{3,"Game (off): Arena"},{4,"Microphone"}});
        QCOMPARE(rows.selectedIds(),QVector<int>({1,2,3,4}));
        pc->click(); QCOMPARE(rows.selectedIds(),QVector<int>({3,4}));
        rows.findChild<QPushButton *>("clipMoreTracks")->click();
        QCOMPARE(rows.selectedIds(),QVector<int>({3,4}));
        QCOMPARE(rows.selectedOrdinals(),QVector<int>({2,3}));
    }
    void dynamicAudioManyApplicationsStayScrollable() {
        QScrollArea scroll; scroll.setWidgetResizable(true); scroll.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *rows=new ClipAudioTracks; scroll.setWidget(rows); scroll.resize(420,230); applyTheme(&scroll);
        QVector<QPair<int,QString>> sources{{1,"PC (other)"},{2,"Game: Arena"},{3,"Microphone"}};
        for(int i=0;i<25;++i) sources.append({i+4,"App: A browser with a long shared application name "+QString::number(i)});
        rows->setTracks(sources); rows->setEditing(true); scroll.show();
        const auto ids=rows->selectedIds(); rows->findChild<QPushButton *>("clipMoreTracks")->click();
        QCoreApplication::processEvents(); QCOMPARE(rows->selectedIds(),ids);
        QVERIFY(rows->height()>scroll.viewport()->height());
        for(auto *selector:rows->findChildren<QCheckBox *>()) {
            auto *row=selector->parentWidget();
            if(!row->isVisible()) { continue; }
            QVERIFY(selector->x()<=8);
            QVERIFY(row->geometry().right()<rows->width());
            QVERIFY(!row->grab().isNull());
        }
        rows->findChild<QCheckBox *>("clipTrack27")->setFocus();
        QTest::keyClick(rows->findChild<QCheckBox *>("clipTrack27"),Qt::Key_Space);
        QCOMPARE(rows->selectedIds().size(),27);
    }
    void recordingPauseControlReflectsActualState() {
        Dashboard dashboard(caps); dashboard.show(); GsrOptions options; options.continuousRecording=true;
        auto *pause=dashboard.findChild<QPushButton *>("pauseRecording"); QVERIFY(pause);
        QSignalSpy requested(&dashboard,&Dashboard::recordingPauseRequested);
        dashboard.updateState(options,{},true,true,false,false);
        QVERIFY(pause->isVisible()); QVERIFY(pause->isEnabled()); QCOMPARE(pause->text(),QString("Pause recording"));
        pause->click(); QCOMPARE(requested.size(),1);
        dashboard.updateState(options,{},true,true,false,false,false,false,true);
        QCOMPARE(pause->text(),QString("Resume recording"));
        QCOMPARE(dashboard.findChild<QLabel *>("statusTitle")->text(),QString("Recording paused"));
        dashboard.updateState(options,{},true,true,false,false,false,false,false,true);
        QVERIFY(!pause->isEnabled()); QCOMPARE(pause->text(),QString("Please wait…"));
        dashboard.updateState(options,{},true,true,false,false,false,false,false,false,false);
        QVERIFY(!pause->isEnabled());
        QCOMPARE(dashboard.findChild<QLabel *>("statusTitle")->text(),QString("Recording pause state unknown"));
        options.continuousRecording=false;
        dashboard.updateState(options,{},true,true,false,false,true);
        QVERIFY(!pause->isVisible());
        StatusIndicator indicator; indicator.show(); indicator.setState(StatusIndicator::Paused);
        QCOMPARE(indicator.accessibleDescription(),QString("Recording paused"));
    }
    void playerFiltersRecentClipsAndKeepsThumbnails() {
        ClipPlayer player;
        const QString root="/tmp/shadowless-filter-clips";
        const QString replay=root+"/Game A/Replay_1.mp4", recording=root+"/Game B/Recording_2.mkv";
        const QString desktop=root+"/Replay_3.mp4", imported=root+"/Game A/imported.mp4";
        player.setClips({replay,recording,desktop,imported},root);
        auto *games=player.findChild<QComboBox *>("clipGameFilter"); QVERIFY(games);
        auto *mode=player.findChild<QComboBox *>("clipModeFilter"); QVERIFY(mode);
        auto *list=player.findChild<QListWidget *>("clipRecent"); QCOMPARE(list->count(),4);
        QPixmap red(48,28); red.fill(Qt::red); player.setThumbnail(replay,red);
        games->setCurrentIndex(games->findText("Game A")); QCOMPARE(list->count(),2);
        mode->setCurrentIndex(mode->findData("recording")); QCOMPARE(list->count(),0);
        mode->setCurrentIndex(mode->findData("replay")); QCOMPARE(list->count(),1);
        QCOMPARE(list->item(0)->data(Qt::UserRole).toString(),replay);
        QCOMPARE(list->item(0)->icon().pixmap(48,28).toImage().pixelColor(24,14),QColor(Qt::red));
        mode->setCurrentIndex(mode->findData("unknown")); QCOMPARE(list->count(),1);
        QCOMPARE(list->item(0)->data(Qt::UserRole).toString(),imported);
        games->setCurrentIndex(0); mode->setCurrentIndex(0); QCOMPARE(list->count(),4);
        games->setCurrentIndex(games->findText("Game B"));
        player.setClips({desktop},root);
        QCOMPARE(games->currentIndex(),0); QCOMPARE(list->count(),1);
        QCOMPARE(games->itemText(1),QString("Desktop / Unidentified"));
    }
    void settingsScrollsThroughCaptureStates() {
        SettingsPage page(caps); applyTheme(&page);
        page.edit(GsrOptions{},false,false,keys); page.show();
        auto *scroll=page.findChild<QScrollArea *>(); QVERIFY(scroll);
        for(const auto &size:{QSize(420,620),QSize(560,740)}) for(bool active:{false,true}) {
            page.resize(size); page.setCaptureActive(active); QCoreApplication::processEvents();
            auto *bar=scroll->verticalScrollBar(); QVERIFY(bar->maximum()>0);
            for(int value=0;value<=bar->maximum();value+=qMax(1,bar->maximum()/8)) {
                bar->setValue(value); QCoreApplication::processEvents();
                QVERIFY(!page.grab().isNull());
                const auto point=scroll->viewport()->rect().center();
                QWheelEvent wheel(point,scroll->viewport()->mapToGlobal(point),QPoint(),QPoint(0,-120),
                    Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                QApplication::sendEvent(scroll->viewport(),&wheel); QCoreApplication::processEvents();
            }
            bar->setValue(bar->maximum()); QVERIFY(page.isVisible());
        }
    }
    void editorFiltersUseWholeLibraryBeforeDashboardLimit() {
        QTemporaryDir directory; QVERIFY(QDir(directory.path()).mkpath("Game A")); QVERIFY(QDir(directory.path()).mkpath("Game B"));
        for(int i=0;i<27;++i) {
            const auto path=directory.filePath(i==26?"Game B/Recording_old.mp4":QString("Game A/Replay_%1.mp4").arg(i));
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("fixture");
        }
        Dashboard dashboard(caps); dashboard.show(); dashboard.refreshLibrary(directory.path());
        auto *player=dashboard.findChild<ClipPlayer *>(); auto *list=player->findChild<QListWidget *>("clipRecent");
        QTRY_VERIFY(list->count()>0); QCOMPARE(list->count(),27);
        auto *library=dashboard.findChild<Library *>();
        auto *dashboardFilter=library->findChild<QComboBox *>("collectionFilter");
        dashboardFilter->setCurrentIndex(dashboardFilter->findData("Game A"));
        QCOMPARE(list->count(),27);
        auto *mode=player->findChild<QComboBox *>("clipModeFilter");
        mode->setCurrentIndex(mode->findData("recording")); QCOMPARE(list->count(),1);
        QCOMPARE(player->findChild<QComboBox *>("clipGameFilter")->findText("Game B")>=0,true);
    }
    void playerDragsVisibleTrimHandles() {
        QTemporaryDir directory; const auto path=directory.filePath("trim.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","8","-c:v","mpeg4",path}),0);
        ClipPlayer player; player.resize(1000,750); player.show(); player.open(path);
        auto *media=player.findChild<MpvPlayer *>();
        auto *tools=player.findChild<MediaTools *>();
        QTRY_VERIFY(media->loaded());
        player.findChild<QPushButton *>("clipTrim")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(),15000);
        auto *timeline=player.findChild<QWidget *>("clipTimeline"); QVERIFY(timeline);
        auto *in=player.findChild<QDoubleSpinBox *>("clipIn");
        auto *out=player.findChild<QDoubleSpinBox *>("clipOut");
        const int y=timeline->height()/2, w=timeline->width();
        const auto drag=[&](int from,int to) {
            QTest::mouseMove(timeline,QPoint(from,y));
            QCOMPARE(timeline->cursor().shape(),Qt::SizeHorCursor);
            QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(from,y));
            QTest::mouseMove(timeline,QPoint(to,y));
            QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(to,y));
        };
        // Grab inside each visible handle, including the initial clip edges.
        drag(12,w/4+12);
        QVERIFY(qAbs(in->value()-2)<.05); QCOMPARE(out->value(),8.);
        QTRY_VERIFY(media->paused()); QTRY_VERIFY(qAbs(media->position()-2000)<150);
        drag(w-12,w*3/4-12);
        QVERIFY(qAbs(out->value()-6)<.05);
        drag(qRound(in->value()/8*w)+8,w);
        QVERIFY(in->value()<out->value());
        in->setValue(2);
        drag(qRound(out->value()/8*w)-8,0);
        QVERIFY(out->value()>in->value());
        in->setValue(0); out->setValue(8);
        drag(12,12); QCOMPARE(in->value(),0.);
        drag(w-12,w-12); QCOMPARE(out->value(),8.);
        QTest::mouseMove(timeline,QPoint(w/2,y));
        QCOMPARE(timeline->cursor().shape(),Qt::PointingHandCursor);
        // Both handles must remain reachable when a short range makes them overlap.
        in->setValue(2); out->setValue(2.+16.*8/w);
        const double before=out->value();
        const int end=qRound(before/8*w)-2;
        drag(end,end+w/4);
        QVERIFY(qAbs(out->value()-(before+2))<.05); QCOMPARE(in->value(),2.);
        in->setValue(1); out->setValue(6);
        QTest::mouseClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(1,y));
        QTRY_VERIFY(qAbs(media->position()-1000)<150);
        QTest::keyClick(media,Qt::Key_Right);
        QTest::keyClick(media,Qt::Key_Right);
        QTRY_VERIFY(qAbs(media->position()-6000)<150);
        player.findChild<QPushButton *>("clipPlayPause")->click();
        QTRY_VERIFY(!media->paused());
        QTRY_VERIFY(media->position()>=1000 && media->position()<3000);
        out->setValue(2);
        QTRY_VERIFY_WITH_TIMEOUT(media->paused(),3000);
        QVERIFY(qAbs(media->position()-2000)<150);
        QTest::qWait(200); QVERIFY(media->position()<=2100);
        out->setValue(2.04);
        media->seek(2040); QTRY_VERIFY(qAbs(media->position()-2100)<50);
        QTest::qWait(150);
        media->seek(1500); media->pause(false);
        QTRY_VERIFY(media->position()>=1500 && media->position()<2000);
        QTRY_VERIFY_WITH_TIMEOUT(media->paused(),3000);
        media->seek(1500); QTRY_VERIFY(qAbs(media->position()-1500)<50);
        QTest::qWait(150); QVERIFY(qAbs(media->position()-1500)<50);
        player.findChild<QPushButton *>("clipTrim")->click();
        media->seek(7000); QTRY_VERIFY(qAbs(media->position()-7000)<150);
        const auto output=qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if(!output.isEmpty()) QVERIFY(timeline->grab().save(output+".timeline.png"));
    }
    void playerInitializesWithRegionalNumericLocale() {
        const QByteArray previous=std::setlocale(LC_NUMERIC,nullptr);
        const auto restore=qScopeGuard([previous] { std::setlocale(LC_NUMERIC,previous.constData()); });
        if(!std::setlocale(LC_NUMERIC,"de_DE.UTF-8"))
            QSKIP("The de_DE.UTF-8 comma-decimal locale is required for this check.");
        QCOMPARE(QByteArray(std::localeconv()->decimal_point),QByteArray(","));
        MpvPlayer player; player.show(); QSignalSpy errors(&player,&MpvPlayer::error);
        player.open("/nonexistent-shadowless-locale-test.mp4");
        QCOMPARE(QByteArray(std::setlocale(LC_NUMERIC,nullptr)),QByteArray("C"));
        for(const auto &error:errors) QVERIFY(!error[0].toString().contains("create the media player"));
        player.release();
    }
    void playerLoadsSidebarThumbnailsAfterLeavingLibrary() {
        QTemporaryDir directory; const auto path=directory.filePath("red.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","2","-c:v","mpeg4",path}),0);
        Dashboard dashboard(caps); dashboard.show();
        auto *library=dashboard.findChild<Library *>();
        auto *player=dashboard.findChild<ClipPlayer *>();
        connect(library,&Library::clipsChanged,this,[library,path](const QStringList &paths) { if(paths.contains(path)) library->clipRequested(path); });
        dashboard.refreshLibrary(directory.path());
        QTRY_VERIFY(player->isVisible()); QVERIFY(!library->isVisible());
        auto *recent=player->findChild<QListWidget *>("clipRecent");
        QTRY_COMPARE(recent->count(),1);
        QTRY_VERIFY(recent->item(0)->icon().pixmap(48,28).toImage().pixelColor(24,14).red()>180);
    }
    void playerNavigationFullscreenAndLazyEditor() {
        QTemporaryDir directory;
        const auto newest = directory.filePath("new.mp4"), oldest = directory.filePath("old.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-t", "4", "-c:v", "mpeg4", newest}), 0);
        QVERIFY(QFile::copy(newest, oldest));
        Dashboard dashboard(caps); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(newest);
        player->setClips({newest, oldest});
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        QSignalSpy metadata(tools, &MediaTools::metadataReady);
        QTRY_VERIFY(media->loaded());
        QVERIFY(!tools->busy()); QCOMPARE(metadata.size(), 0);
        QVERIFY(!player->findChild<QPushButton *>("clipNext")->isEnabled());
        QVERIFY(player->findChild<QPushButton *>("clipPrevious")->isEnabled());
        player->findChild<QPushButton *>("clipFullscreen")->click();
        QVERIFY(player->fullscreen());
        QCOMPARE(dashboard.findChild<QWidget *>("shadowlessPanel")->geometry(), dashboard.rect());
        QVERIFY(!player->findChild<QPushButton *>("clipTrim")->isVisible());
        QVERIFY(!player->findChild<QListWidget *>()->isVisible());
        dashboard.escape(); QVERIFY(!player->fullscreen()); QVERIFY(player->isVisible());
        player->findChild<QPushButton *>("clipPrevious")->click();
        QTRY_VERIFY(media->loaded()); QCOMPARE(media->source(), oldest);
        QVERIFY(!player->findChild<QPushButton *>("clipPrevious")->isEnabled());
        auto *recent=player->findChild<QListWidget *>("clipRecent");
        recent->setFocus(); QTest::keyClick(recent,Qt::Key_Up); QCOMPARE(recent->currentRow(),0);
        QTest::qWait(250); QCOMPARE(recent->currentRow(),0);
        QTest::keyClick(recent,Qt::Key_Return); QTRY_COMPARE(media->source(),newest);
        auto *speed = player->findChild<QComboBox *>("clipSpeed");
        QTest::keyClick(media, Qt::Key_Up); QCOMPARE(speed->currentData().toDouble(), 1.25);
        player->findChild<QPushButton *>("clipTrim")->click();
        QVERIFY(tools->busy());
        dashboard.escape();
        QTRY_VERIFY(!tools->busy()); QVERIFY(media->source().isEmpty());
        QTest::qWait(100); QCOMPARE(metadata.size(), 0);
    }
    void playerAudioEditorAndPickerLifecycle() {
        QTemporaryDir directory;
        const auto path = directory.filePath("Four audio tracks.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x180:r=15", "-f", "lavfi", "-i", "sine=frequency=440", "-f", "lavfi", "-i", "sine=frequency=880", "-f", "lavfi", "-i", "sine=frequency=1320", "-f", "lavfi", "-i", "sine=frequency=1760", "-t", "8", "-map", "0:v", "-map", "1:a", "-map", "2:a", "-map", "3:a", "-map", "4:a", "-metadata:s:a:0", "handler_name=Game", "-metadata:s:a:1", "handler_name=Music", "-metadata:s:a:2", "handler_name=PC", "-metadata:s:a:3", "handler_name=Microphone", "-c:v", "libx264", "-threads", "1", "-c:a", "aac", path}), 0);
        Dashboard dashboard(caps); dashboard.resize(1280, 900); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        QTRY_VERIFY(media->loaded()); media->setMuted(true);
        QTRY_COMPARE(media->audioTracks().size(), 4);
        QTRY_COMPARE(player->findChildren<QCheckBox *>(QRegularExpression("clipTrack.*")).size(), 4);
        QVERIFY(!tools->busy());
        media->pause(true); QTRY_VERIFY(media->paused());
        auto *second = player->findChild<QCheckBox *>("clipTrack1"); QVERIFY(second); second->setChecked(false);
        QTest::qWait(100); QVERIFY(!second->isChecked());
        player->findChild<QPushButton *>("clipTrim")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(), 15000);
        auto *audio = player->findChild<QScrollArea *>("clipAudioTracks"); QVERIFY(audio);
        for (int i=0;i<4;++i) {
            auto *track=player->findChild<QCheckBox *>(QString("clipTrack%1").arg(i)); QVERIFY(track);
            QVERIFY(track->isVisible());
            QVERIFY2(audio->viewport()->rect().contains(QRect(track->mapTo(audio->viewport(),QPoint()),track->size())), "Every track toggle must fit without scrolling");
            QVERIFY(track->height() >= 32);
            QCOMPARE(track->accessibleName(),QStringList({"Game","Music","PC","Microphone"})[i]);
        }
        auto *exportButton = player->findChild<QPushButton *>("clipExport");
        auto *timeline = player->findChild<QWidget *>("clipTimeline"); QVERIFY(timeline);
        QVERIFY(timeline->mapTo(player, QPoint()).y() < audio->mapTo(player, QPoint()).y());
        QVERIFY(exportButton->mapTo(player, QPoint()).y() > timeline->mapTo(player, QPoint()).y());
        QVERIFY(exportButton->isEnabled());
        auto *in = player->findChild<QDoubleSpinBox *>("clipIn"), *out = player->findChild<QDoubleSpinBox *>("clipOut");
        in->setFocus(); in->selectAll(); QTest::keyClicks(in,"1.5"); QTest::keyClick(in,Qt::Key_Return);
        QCOMPARE(in->value(),1.5); QCOMPARE(in->text(),QString("00:01.500"));
        in->setFocus(); in->selectAll(); QTest::keyClicks(in,"00:01.250"); QTest::keyClick(in,Qt::Key_Return);
        QCOMPARE(in->value(),1.25); QCOMPARE(in->text(),QString("00:01.250"));
        out->setValue(3); QVERIFY(exportButton->isEnabled());
        const int rowHeight=second->parentWidget()->height();
        QTest::mouseClick(second,Qt::LeftButton,Qt::NoModifier,QPoint(second->width()-8,second->height()/2));
        QVERIFY(second->isChecked()); QTest::qWait(200);
        QCOMPARE(second->parentWidget()->height(),rowHeight);
        second->setFocus(); QTest::keyClick(second,Qt::Key_Space); QVERIFY(!second->isChecked());
        QTest::qWait(200); QCOMPARE(second->parentWidget()->height(),rowHeight);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output + ".editor.png"));
        player->setFullscreen(true); QVERIFY(!in->isVisible());
        dashboard.escape(); QVERIFY(in->isVisible());
        exportButton->click();
        QTRY_VERIFY(player->findChild<QFileDialog *>()->isVisible());
        dashboard.escape(); QVERIFY(player->isVisible()); QVERIFY(in->isVisible());
        dashboard.escape(); QVERIFY(media->source().isEmpty()); QVERIFY(!tools->busy());
    }
    void playerExportSurvivesHidingOverlay_data() {
        QTest::addColumn<int>("outcome");
        QTest::newRow("complete") << 0;
        QTest::newRow("cancel") << 1;
        QTest::newRow("failure") << 2;
    }
    void playerExportSurvivesHidingOverlay() {
        QFETCH(int, outcome);
        QTemporaryDir directory;
        const auto path = directory.filePath("source.mp4"), output = directory.filePath("source_trim.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x180:r=15", "-t", "4", "-c:v", "libx264", "-threads", "1", path}), 0);
        Dashboard dashboard(caps); dashboard.resize(1280, 900); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        QTRY_VERIFY(media->loaded());
        player->findChild<QPushButton *>("clipTrim")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(), 15000);
        player->findChild<QPushButton *>("clipExport")->click();
        auto *picker = player->findChild<QFileDialog *>(); QVERIFY(picker);
        QCOMPARE(picker->selectedFiles().value(0), output);
        QSignalSpy exported(tools, &MediaTools::exported), cancelled(tools, &MediaTools::cancelled), failed(tools, &MediaTools::failed);
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QVERIFY(tools->busy());
        auto *rendering = dashboard.findChild<QLabel *>("clipRenderingStatus");
        QVERIFY(rendering); QCOMPARE(rendering->text(), "rendering...");
        QVERIFY(rendering->isVisible());
        dashboard.hide(); dashboard.home();
        QVERIFY(media->source().isEmpty());
        QCOMPARE(cancelled.size(), 0);
        dashboard.show();
        QVERIFY(rendering->isVisible());
        dashboard.findChild<Library *>()->clipRequested(path);
        QCOMPARE(cancelled.size(), 0);
        if (outcome) {
            if (outcome == 1) tools->stop();
            else QVERIFY(QFile::remove(path));
            QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(), 15000);
            QVERIFY(!rendering->isVisible());
            QCOMPARE(exported.size(), 0);
            QCOMPARE(cancelled.size(), outcome == 1 ? 1 : 0);
            QCOMPARE(failed.size(), outcome == 2 ? 1 : 0);
            QVERIFY(!QFileInfo::exists(output));
            return;
        }
        QTRY_COMPARE_WITH_TIMEOUT(exported.size(), 1, 15000);
        QVERIFY(!rendering->isVisible());
        QCOMPARE(exported[0][0].toString(), output);
        QVERIFY(QFileInfo::exists(output));
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-i", output, "-f", "null", "-"}), 0);
    }
    void playerResourceSample() {
        const auto source = qEnvironmentVariable("SHADOWLESS_PLAYER_SAMPLE");
        if (source.isEmpty()) QSKIP("Set SHADOWLESS_PLAYER_SAMPLE to a generated fixture for the opt-in resource sample.");
        const auto output = qEnvironmentVariable("SHADOWLESS_PLAYER_SAMPLE_OUTPUT");
        QVERIFY(!output.isEmpty());
        const auto mark = [&](const QByteArray &phase) { QFile file(output + ".phase"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(phase); };
        Dashboard dashboard(caps); dashboard.resize(1280, 720); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        mark("idle"); QTest::qWait(3500);
        dashboard.findChild<Library *>()->clipRequested(source); media->setMuted(true);
        QTRY_VERIFY(media->loaded()); QTRY_VERIFY(media->position() > 100);
        QSignalSpy frames(media, &MpvPlayer::frameRendered);
        mark("playing"); QTest::qWait(4000);
        const int playingFrames = frames.size(); QVERIFY(playingFrames > 20); QVERIFY(!tools->busy());
        const auto decoder = media->hardwareDecoder();
        dashboard.hide(); QVERIFY(media->source().isEmpty()); QTest::qWait(250);
        const int hiddenFrames = frames.size();
        mark("hidden"); QTest::qWait(3500);
        QCOMPARE(frames.size(), hiddenFrames); QVERIFY(!tools->busy());
        QFile result(output + ".json"); QVERIFY(result.open(QIODevice::WriteOnly));
        result.write(QJsonDocument(QJsonObject{{"playing_frames", playingFrames}, {"hidden_frames", frames.size() - hiddenFrames}, {"analysis_running", tools->busy()}, {"hardware_decoder", decoder}}).toJson());
        mark("done");
    }
    void showsRecordingAlongsideReplay() {
        Dashboard dashboard(caps);
        dashboard.show();
        dashboard.updateState(GsrOptions{}, {}, true, true, false, false);
        const auto recording = button(&dashboard, "Recording");
        QVERIFY(recording && recording->isVisible() && recording->isEnabled());
        QVERIFY(!dashboard.findChild<QPushButton *>("captureAction")); // the Instant Clip card is the only save control
    }
    void exposesCaptureDeviceSelectors() {
        SettingsPage page(caps);
        const auto output = page.findChild<QComboBox *>("audioOutput");
        const auto input = page.findChild<QComboBox *>("audioInput");
        QVERIFY(output && input);
        QCOMPARE(output->currentText(), "Default output");
        QCOMPARE(input->currentText(), "Default input");
    }
    void savesPcTrackFromAudioSettings() {
        SettingsPage page(caps); GsrOptions options;
        options.source = "DP-1"; options.outputDirectory = "/tmp/clips";
        options.applicationAudio = true; options.applicationTracks = {{"Game", 1}, {"Spotify", 2}};
        options.pcTrack = 3; page.edit(options, false, false, keys); page.showAudio(); page.show();
        auto *pc = page.findChild<QComboBox *>("pcAudioTrack"); QVERIFY(pc);
        QVERIFY(pc->isVisible()); QCOMPARE(pc->currentIndex(), 3);
        pc->setCurrentIndex(5);
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 1);
        const auto result = qvariant_cast<GsrOptions>(saved[0][0]); QCOMPARE(result.pcTrack, 5);
        QCOMPARE(result.applicationTracks, options.applicationTracks);
    }
    void fourTrackDashboardHasIndependentRoleLevels() {
        Dashboard dashboard(caps); dashboard.show();
        GsrOptions options; options.applicationAudio = true; options.fourTrackAudio = true;
        options.microphone = true; options.pcTrack = 1; options.microphoneTrack = 4;
        options.levels.systemVolume = 80; options.levels.gameVolume = 45;
        options.levels.musicVolume = 30; options.levels.musicMuted = true;
        dashboard.updateState(options, {}, true, true, false, false);
        auto *game = dashboard.findChild<QSlider *>("gameCaptureVolume");
        auto *music = dashboard.findChild<QSlider *>("musicCaptureVolume");
        auto *gameButton = dashboard.findChild<QPushButton *>("gameCaptureAudio");
        auto *musicButton = dashboard.findChild<QPushButton *>("musicCaptureAudio");
        // Sliders live in each row's "100% ▾" popup; the row buttons are what the dashboard shows.
        QVERIFY(game && music && gameButton && musicButton && gameButton->isVisible() && musicButton->isVisible());
        QCOMPARE(game->value(), 45); QCOMPARE(music->value(), 30);
        QVERIFY(gameButton && musicButton); QVERIFY(gameButton->isChecked()); QVERIFY(!musicButton->isChecked());
        QVERIFY(button(&dashboard, "PC")->isVisible());
        auto *summary = dashboard.findChild<QLabel *>("audioTrackSummary"); QVERIFY(summary);
        QCOMPARE(summary->text(), QString("Audio tracks: PC · Game · Music · Microphone"));
        QVERIFY(summary->wordWrap());
        QSignalSpy volumes(&dashboard, &Dashboard::roleVolumeChanged);
        QSignalSpy audio(&dashboard, &Dashboard::roleAudioChanged);
        QSignalSpy system(&dashboard, &Dashboard::volumeChanged);
        game->setValue(35); music->setValue(20); musicButton->click();
        QCOMPARE(volumes.size(), 2); QCOMPARE(volumes[0][0].toInt(), 2); QCOMPARE(volumes[0][1].toInt(), 35);
        QCOMPARE(volumes[1][0].toInt(), 3); QCOMPARE(volumes[1][1].toInt(), 20);
        QCOMPARE(audio.size(), 1); QCOMPARE(audio[0][0].toInt(), 3); QVERIFY(audio[0][1].toBool());
        QVERIFY(system.isEmpty());
        dashboard.updateState(options, {}, true, true, false, false);
        QCOMPARE(volumes.size(), 2);
        options.fourTrackAudio = false; options.dynamicApplicationAudio = false; // legacy two-source mode; dynamic grouping is the default
        options.applicationAudio = false; options.separateAudioTracks = true;
        dashboard.updateState(options, {}, true, true, false, false);
        QVERIFY(!game->isVisible() && !music->isVisible());
        QCOMPARE(summary->text(), QString("Audio tracks: PC · Microphone"));
        options.separateAudioTracks = false;
        dashboard.updateState(options, {}, true, true, false, false);
        QCOMPARE(summary->text(), QString("Audio tracks: PC + Microphone (mixed)"));
        auto *setup = dashboard.findChild<QPushButton *>("fourTrackSetup");
        QVERIFY(setup && setup->isVisible() && !setup->isEnabled());
        dashboard.updateState(options, {}, false, false, false, false);
        QVERIFY(setup->isEnabled());
        QSignalSpy setupRequested(&dashboard, &Dashboard::fourTrackSetupRequested);
        setup->click(); QCOMPARE(setupRequested.size(), 1);
        QTemporaryDir directory; Settings settings(directory.filePath("settings.ini")); QString error;
        const auto defaults = settings.capture(caps);
        QCOMPARE(defaults.levels.gameVolume, 100); QCOMPARE(defaults.levels.musicVolume, 100);
        QVERIFY(!defaults.levels.gameMuted && !defaults.levels.musicMuted);
        options.levels.gameMuted = true;
        QVERIFY(settings.save(options, {}, false, &error));
        const auto restored = settings.capture(caps);
        QCOMPARE(restored.levels.gameVolume, 45); QCOMPARE(restored.levels.musicVolume, 30);
        QVERIFY(restored.levels.gameMuted && restored.levels.musicMuted);
        QCOMPARE(restored.levels.systemVolume, 80);
    }
    void fourTrackAudioSetupPersistsNamedRoles() {
        SettingsPage page(caps); GsrOptions options;
        options.source = "DP-1"; options.outputDirectory = "/tmp/clips";
        options.applicationAudio = true;
        options.applicationTracks = {{"Game application", 2}, {"Music application", 3}};
        page.edit(options, false, false, keys);
        auto *mode = page.findChild<QComboBox *>("audioRoutingMode"); QVERIFY(mode);
        mode->setCurrentIndex(2);
        auto *table = page.findChild<QTableWidget *>(); QVERIFY(table);
        QCOMPARE(table->rowCount(), 2);
        auto *game = qobject_cast<QComboBox *>(table->cellWidget(0, 1)); QVERIFY(game);
        auto *music = qobject_cast<QComboBox *>(table->cellWidget(1, 1)); QVERIFY(music);
        QCOMPARE(game->currentText(), "Game (track 2)");
        QCOMPARE(music->currentText(), "Music (track 3)");
        QCOMPARE(game->count(), 3);
        QSignalSpy saved(&page, &SettingsPage::saved);
        music->setCurrentIndex(0);
        button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 0);
        QVERIFY(page.findChild<QLabel *>("settingsError")->text().contains("music"));
        music->setCurrentIndex(music->findData(3));
        button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 1);
        const auto result = qvariant_cast<GsrOptions>(saved[0][0]);
        QVERIFY(result.applicationAudio && result.fourTrackAudio && result.microphone);
        QCOMPARE(result.pcTrack, 1); QCOMPARE(result.microphoneTrack, 4);
        QCOMPARE(result.applicationTracks, options.applicationTracks);
        QCOMPARE(result.audioTracks().size(), 4);
        QTemporaryDir directory; Settings settings(directory.filePath("settings.ini")); QString error;
        QVERIFY(settings.save(result, {}, false, &error));
        const auto restored = settings.capture(caps);
        QVERIFY(restored.fourTrackAudio); QCOMPARE(restored.audioTracks(), result.audioTracks());
        page.edit(restored, false, false, keys);
        QCOMPARE(mode->currentIndex(), 2);
        mode->setCurrentIndex(1);
        button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 2);
        QVERIFY(!qvariant_cast<GsrOptions>(saved[1][0]).fourTrackAudio);
        QVERIFY(settings.save(options, {}, false, &error));
        QVERIFY(!settings.capture(caps).fourTrackAudio);
    }
    void videoPresetsPreserveCustomValuesAndIgnoreWheel() {
        SettingsPage page(caps);
        GsrOptions original;
        original.source = "DP-1";
        original.outputDirectory = "/tmp/clips";
        original.frameRate = 75;
        original.bitrateMbps = 27;
        page.edit(original, false, false, keys);

        for (const auto *name : {"captureFps", "captureBitrate"}) {
            auto *selector = page.findChild<QComboBox *>(name);
            QVERIFY(selector);
            QVERIFY(!selector->isEditable());
            QCOMPARE(selector->currentText(), "Custom");
            selector->setCurrentIndex(1);
            QWheelEvent wheel(QPointF(4, 4), QPointF(4, 4), {}, QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(selector, &wheel);
            QCOMPARE(selector->currentIndex(), 1);
            selector->setCurrentIndex(selector->count() - 1);
        }

        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1);
        const auto options = qvariant_cast<GsrOptions>(saved.first().first());
        QCOMPARE(options.frameRate, 75);
        QCOMPARE(options.bitrateMbps, 27);
    }
    void customHexAccentPersistsAndRejectsInvalidInput() {
        QTemporaryDir directory;
        Settings settings(directory.filePath("settings.ini"));
        SettingsPage page(caps);
        GsrOptions options; options.source = "DP-1"; options.outputDirectory = directory.path();
        page.edit(options, false, false, keys, "#39a7f0");
        auto *selector = page.findChild<QComboBox *>("accentColor");
        auto *hex = page.findChild<QLineEdit *>("customAccent");
        QVERIFY(hex); QCOMPARE(hex->text(), "#39a7f0");
        QCOMPARE(selector->currentData().toString(), "custom");
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1); QCOMPARE(saved.first()[3].toString(), "#39a7f0");
        QString error;
        QVERIFY(settings.save(options, {}, false, &error, keys, saved.first()[3].toString()));
        QCOMPARE(Settings(directory.filePath("settings.ini")).accent(), "#39a7f0");
        Dashboard dashboard(caps); dashboard.setAccent(settings.accent());
        QCOMPARE(dashboard.palette().color(QPalette::Accent), QColor("#39a7f0"));
        QVERIFY(!dashboard.windowIcon().pixmap(22, 22).isNull());
        hex->setText("#xyz"); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 1);
        selector->setCurrentIndex(selector->findData("mint"));
        button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 2);
        QCOMPARE(saved.last()[3].toString(), "mint");
    }
    void customAccentsKeepReadableForegrounds() {
        for (const auto &value : {"#000000", "#ffffff", "#39a7f0"}) {
            QWidget widget; applyTheme(&widget, value);
            QCOMPARE(accentColor(value), QColor(value));
            const auto foreground = widget.palette().color(QPalette::HighlightedText);
            const auto background = widget.palette().color(QPalette::Highlight);
            QVERIFY(qAbs(qGray(foreground.rgb()) - qGray(background.rgb())) >= 100);
        }
        QCOMPARE(accentColor("#xyz"), accentColor("violet"));
    }
    void dialogsRemainReadableWithLightDesktopPalette() {
        QWidget root;
        QPalette light;
        light.setColor(QPalette::Window, Qt::white);
        light.setColor(QPalette::Base, Qt::white);
        light.setColor(QPalette::AlternateBase, QColor("#eeeeee"));
        light.setColor(QPalette::Button, Qt::white);
        root.setPalette(light);
        applyTheme(&root);
        QFileDialog picker(&root);
        picker.setOption(QFileDialog::DontUseNativeDialog);
        picker.resize(600, 400); picker.show();
        QMessageBox message(QMessageBox::Question, "Update", "Download update?", QMessageBox::Yes | QMessageBox::No, &root);
        message.show(); QCoreApplication::processEvents();
        for (auto *view : picker.findChildren<QAbstractItemView *>()) {
            if (!view->isVisible()) continue;
            const auto image = view->viewport()->grab().toImage();
            QVERIFY(!image.isNull());
            QVERIFY2(image.pixelColor(image.width() - 2, image.height() - 2).lightness() < 100,
                     "File views must not inherit a white desktop background under light text");
        }
        QVERIFY(message.grab().toImage().pixelColor(2, 2).lightness() < 100);
        QVERIFY(message.palette().color(QPalette::Button).lightness() < 100);
    }
    void customAccentButtonStatesRemainReadable() {
        const auto luminance = [](const QColor &color) {
            const auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
            return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
        };
        const auto contrast = [&](const QColor &a, const QColor &b) {
            const auto x = luminance(a), y = luminance(b);
            return (qMax(x, y) + .05) / (qMin(x, y) + .05);
        };
        for (const auto &accent : {"#757575", "#808080", "#39a7f0", "#ffffff"}) {
            QPushButton action("MMMM"); action.setObjectName("primary");
            applyTheme(&action, accent); action.resize(200, 50); action.show();
            action.clearFocus(); QCoreApplication::processEvents();
            QColor baseFill;
            for (int state : {0, 1, 2}) {
                QTest::mouseMove(&action, state == 1 ? QPoint(100, 25) : QPoint(-10, -10));
                action.setAttribute(Qt::WA_UnderMouse, state == 1);
                action.setDown(state == 2); QCoreApplication::processEvents();
                const auto image = action.grab().toImage();
                const auto fill = image.pixelColor(20, 25);
                if (state == 0) baseFill = fill;
                else if (state != 1 || QString(accent) != "#ffffff")
                    QVERIFY2(fill != baseFill, "The rendered button must enter its hover/pressed state.");
                double strongest = 1;
                for (int y = 17; y < 33; ++y) for (int x = 70; x < 130; ++x)
                    strongest = qMax(strongest, contrast(fill, image.pixelColor(x, y)));
                QVERIFY2(strongest >= 4.5, accent);
            }
            action.setAttribute(Qt::WA_UnderMouse, false);
            action.setDown(false); action.clearFocus();
            const auto unfocused = action.grab().toImage();
            action.setFocus(); QCoreApplication::processEvents();
            QVERIFY2(unfocused != action.grab().toImage(), accent);
        }
    }
    void violetIsDefaultAndSavedAccentsArePreserved() {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.ini");
        Settings settings(path);
        QCOMPARE(settings.accent(), "violet");
        Dashboard dashboard(caps);
        QCOMPARE(dashboard.palette().color(QPalette::Accent), QColor("#b69af7"));
        QCOMPARE(accentColor("unknown"), QColor("#b69af7"));
        QString error;
        QVERIFY(settings.save(GsrOptions{}, {}, false, &error, keys, "mint"));
        QCOMPARE(Settings(path).accent(), "mint");
    }
    void migratesLegacySettingsAndPersistsAccent() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        QSettings legacy("Captail", "LinuxGsrSpike");
        legacy.setValue("fps", 75);
        legacy.setValue("outputDirectory", "/tmp/existing-clips");
        legacy.sync();

        Settings settings;
        const auto options = settings.capture(caps);
        QCOMPARE(options.frameRate, 75);
        QCOMPARE(options.outputDirectory, "/tmp/existing-clips");
        QCOMPARE(settings.accent(), "violet");

        SettingsPage page(caps);
        page.edit(options, false, false, keys, settings.accent());
        const auto selector = page.findChild<QComboBox *>("accentColor");
        QVERIFY(selector);
        QCOMPARE(selector->count(), 6);
        selector->setCurrentIndex(selector->findData("violet"));
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first()[3].toString(), "violet");

        QString error;
        QVERIFY(settings.save(options, {}, false, &error, keys, saved.first()[3].toString()));
        QCOMPARE(Settings().accent(), "violet");
        QCOMPARE(legacy.value("fps").toInt(), 75);
        Dashboard dashboard(caps);
        dashboard.setAccent(Settings().accent());
        QCOMPARE(dashboard.palette().color(QPalette::Accent), QColor("#b69af7"));
        QVERIFY(!dashboard.windowIcon().pixmap(22, 22).isNull());
    }
    void dashboardKeepsRecordingVolumeAvailableDuringCapture() {
        Dashboard dashboard(caps);
        GsrOptions options; options.dynamicApplicationAudio = false; // legacy "System audio" label
        options.levels.systemVolume = 63;
        options.levels.systemMuted = true;
        dashboard.updateState(options, {}, true, true, false, false);
        const auto system = button(&dashboard, "System audio");
        QVERIFY(system && system->isEnabled() && !system->isChecked());
        QSignalSpy levels(&dashboard, &Dashboard::volumeChanged);
        for (auto *slider : dashboard.findChildren<QSlider *>()) {
            if (slider->accessibleName() != "System audio capture volume") continue;
            QCOMPARE(slider->value(), 63);
            slider->setValue(46);
        }
        QCOMPARE(levels.size(), 1);
        QCOMPARE(levels[0][0].toBool(), false);
        QCOMPARE(levels[0][1].toInt(), 46);
    }
    void savesAudioDevicesAndMixAcrossLaunches() {
        QTemporaryDir directory;
        const auto file = directory.filePath("settings.conf");
        GsrOptions options;
        options.audioOutput = "speakers.monitor";
        options.audioInput = "mic";
        options.applicationAudio = true;
        options.microphoneTrack = 4;
        options.pcTrack = 3;
        options.applicationTracks = {{"Game", 1}, {"Voice chat", 4}};
        options.levels.microphoneMuted = true;
        options.levels.systemVolume = 73;
        QString error;
        QVERIFY(Settings(file).save(options, {}, false, &error));
        const auto restored = Settings(file).capture(caps);
        QCOMPARE(restored.audioOutput, "speakers.monitor");
        QCOMPARE(restored.audioInput, "mic");
        QCOMPARE(restored.applicationTracks, options.applicationTracks);
        QCOMPARE(restored.microphoneTrack, 4);
        QCOMPARE(restored.pcTrack, 3);
        QCOMPARE(restored.levels.systemVolume, 73);
        QVERIFY(restored.applicationAudio && restored.levels.microphoneMuted);
    }
    void dashboardEscapeReturnsBeforeHiding() {
        Dashboard dashboard(caps); dashboard.resize(1280, 720); dashboard.show();
        QSignalSpy hidden(&dashboard, &Dashboard::hideRequested);
        QTest::mouseClick(dashboard.findChild<QPushButton *>("openSettings"), Qt::LeftButton);
        dashboard.escape(); QCOMPARE(hidden.size(), 0);
        dashboard.escape(); QCOMPARE(hidden.size(), 1);
        dashboard.updateState(GsrOptions{}, OverlayScene{}, true, true, false, false);
        QVERIFY(button(&dashboard, "Instant Clip")->isEnabled());
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) {
            GsrOptions options;
            options.source = "DP-1";
            options.outputDirectory = qEnvironmentVariable("SHADOWLESS_UI_FIXTURES");
            dashboard.updateState(options, {}, true, true, false, false);
            auto *library = dashboard.findChild<Library *>();
            dashboard.hide();
            dashboard.refreshLibrary(options.outputDirectory);
            QSignalSpy scanned(library, &Library::storageChanged);
            for (int pass = 1; pass <= 3; ++pass) {
                dashboard.hide();
                dashboard.refreshLibrary(options.outputDirectory);
                dashboard.show();
                QTRY_COMPARE(scanned.size(), pass);
                if (!options.outputDirectory.isEmpty()) {
                    const auto thumbnails = library->findChildren<QLabel *>("thumbnail");
                    QVERIFY(!thumbnails.isEmpty());
                    QTRY_VERIFY(std::all_of(thumbnails.cbegin(), thumbnails.cend(), [](auto *thumbnail) {
                        return !thumbnail->pixmap().isNull();
                    }));
                }
            }
            auto *panel = dashboard.findChild<QWidget *>("shadowlessPanel");
            QVERIFY(panel->grab().save(output));
            dashboard.settingsPage()->edit(options, false, false, keys);
            dashboard.findChild<QPushButton *>("openSettings")->click();
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".settings.png"));
            dashboard.home();
            button(&dashboard, "Audio settings")->click();
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".audio.png"));
            dashboard.home();
            dashboard.findChild<QPushButton *>("openOverlays")->click();
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".overlays.png"));
            dashboard.home();
            dashboard.resize(740, 720);
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".narrow.png"));
        }
    }
    void dashboardModeButtonsToggleCaptureWithoutSelectingTabs() {
        Dashboard dashboard(caps); dashboard.show();
        GsrOptions options; options.source = "DP-1"; options.bufferSeconds = 4;
        auto *replay = button(&dashboard, "Instant Clip");
        auto *record = button(&dashboard, "Recording");
        QVERIFY(replay && record);
        QVERIFY(!dashboard.findChild<QPushButton *>("captureAction"));
        QVERIFY(!dashboard.findChild<QPushButton *>("captureToggle"));
        // Tiles are menu buttons: the first menu entry starts / stops the mode.
        auto choose = [](QPushButton *tile) { auto *menu = tile->findChild<QMenu *>("captureMenu"); QVERIFY(menu); auto *first = menu->actions().first(); if (first->isEnabled()) first->trigger(); };
        QSignalSpy modes(&dashboard, &Dashboard::recordingMode);
        QSignalSpy saved(&dashboard, &Dashboard::saveReplay);
        dashboard.updateState(options, {}, false, false, false, false);
        QVERIFY(!replay->isChecked() && !record->isChecked());
        choose(replay);
        QCOMPARE(modes.size(), 1); QCOMPARE(modes.last().at(0).toBool(), false);
        QVERIFY(!replay->isChecked());
        choose(record);
        QCOMPARE(modes.size(), 2); QCOMPARE(modes.last().at(0).toBool(), true);
        dashboard.updateState(options, {}, true, false, false, false);
        QVERIFY(replay->isEnabled() && !record->isEnabled());
        choose(replay); QCOMPARE(modes.size(), 3);
        dashboard.updateState(options, {}, true, true, false, false);
        QVERIFY(replay->isChecked() && !record->isChecked());
        QVERIFY(replay->isEnabled() && record->isEnabled());
        { auto *menu = replay->findChild<QMenu *>("captureMenu"); auto *save = menu->actions().at(1);
          QVERIFY(save->isEnabled()); save->trigger(); QCOMPARE(saved.size(), 1); QVERIFY(save->text().contains("4 sec")); }
        dashboard.updateState(options, {}, true, true, false, false, true);
        QVERIFY(replay->isChecked() && record->isChecked());
        choose(record); QCOMPARE(modes.size(), 4); QVERIFY(record->isChecked());
        dashboard.updateState(options, {}, true, true, false, false, true, true);
        QVERIFY(!replay->isEnabled() && !record->isEnabled());
        options.continuousRecording = true;
        dashboard.updateState(options, {}, true, true, false, false);
        QVERIFY(!replay->isChecked() && record->isChecked());
        QVERIFY(!replay->isEnabled() && record->isEnabled());
        QVERIFY(!replay->findChild<QMenu *>("captureMenu")->actions().at(1)->isEnabled()); // no replay buffer to save while recording continuously
        choose(record); QCOMPARE(modes.size(), 5);
        dashboard.updateState(options, {}, true, true, false, true);
        QVERIFY(!replay->isEnabled() && !record->isEnabled());
    }
    void overlayShortcutHidesSettingsAndEditor() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = directory.path();
        QString error;
        QVERIFY(Settings().save(options, {}, false, &error, {"Ctrl+J", keys[1], keys[2], keys[3]}));
        Controller controller(caps, {}, false);
        Dashboard *dashboard = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *candidate = qobject_cast<Dashboard *>(widget)) dashboard = candidate;
        }
        QVERIFY(dashboard);
        auto *settings = dashboard->findChild<QPushButton *>("openSettings");
        auto *overlays = dashboard->findChild<QPushButton *>("openOverlays");
        settings->click();
        QVERIFY(dashboard->settingsPage()->isVisible());
        auto *shortcutField = dashboard->settingsPage()->findChild<QKeySequenceEdit *>();
        QVERIFY(shortcutField);
        shortcutField->setFocus();
        QTest::keyClick(shortcutField, Qt::Key_J, Qt::ControlModifier);
        QVERIFY(!dashboard->isVisible());
        controller.command("toggle");
        QVERIFY(overlays->isVisible());
        overlays->click();
        button(dashboard->overlayEditor(), "Browse…")->click();
        auto *picker = dashboard->overlayEditor()->findChild<QFileDialog *>();
        QVERIFY(picker->isVisible());
        QTest::keyClick(picker, Qt::Key_J, Qt::ControlModifier);
        QVERIFY(!dashboard->isVisible());
        controller.command("toggle");
        QVERIFY(overlays->isVisible());
        overlays->click();
        QVERIFY(!picker->isVisible());
    }
    void restoresAltZAfterChangingOverlayShortcut() {
        Dashboard dashboard(caps);
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = "/tmp/clips";
        dashboard.show();
        QSignalSpy saved(dashboard.settingsPage(), &SettingsPage::saved);
        for (const auto &combination : {QString("Alt+O"), QString("Alt+Z")}) {
            const auto current = saved.isEmpty() ? keys : saved.last()[2].toStringList();
            dashboard.setShortcutKeys(current);
            dashboard.settingsPage()->edit(options, false, false, current);
            dashboard.findChild<QPushButton *>("openSettings")->click();
            auto *field = dashboard.settingsPage()->findChild<QKeySequenceEdit *>();
            field->clear();
            field->setFocus();
            QTest::keySequence(field, QKeySequence(combination));
            QCOMPARE(field->keySequence(), QKeySequence(combination));
            const int previous = saved.size();
            button(dashboard.settingsPage(), "Save settings")->click();
            QCOMPARE(saved.size(), previous + 1);
            QCOMPARE(saved.last()[2].toStringList().first(), combination);
            dashboard.home();
        }
    }
    void indicatorAnimatesOnlyWhenVisibleAndConfirmsSave() {
        StatusIndicator indicator;
        auto *animation = indicator.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        indicator.show();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        const auto firstFrame = indicator.grab().toImage();
        QTest::qWait(120);
        QVERIFY(firstFrame != indicator.grab().toImage());
        indicator.saved();
        QCOMPARE(indicator.accessibleDescription(), QString("Clip saved"));
        indicator.setState(StatusIndicator::Recording);
        QCOMPARE(indicator.accessibleDescription(), QString("Clip saved"));
        QTRY_COMPARE(indicator.accessibleDescription(), QString("Recording"));
        indicator.hide();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        indicator.show();
        indicator.saved();
        indicator.hide();
        indicator.show();
        QCOMPARE(indicator.accessibleDescription(), QString("Recording"));
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
    }
    void overlayCloseButtonReopensDashboard() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        Controller controller(caps, {}, false);
        Dashboard *dashboard = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *candidate = qobject_cast<Dashboard *>(widget)) dashboard = candidate;
        }
        QVERIFY(dashboard);
        auto *overlays = dashboard->findChild<QPushButton *>("openOverlays");
        overlays->click();
        QVERIFY(dashboard->overlayEditor()->isVisible());
        dashboard->findChild<QPushButton *>("hideOverlay")->click();
        QVERIFY(!dashboard->isVisible());
        controller.command("toggle");
        QVERIFY(overlays->isVisible());
    }
    void settingsCancelAndValidationPreserveStagedValues() {
        SettingsPage page(caps); GsrOptions original; original.source = "DP-1"; original.outputDirectory = "/tmp/clips";
        original.bitrateMbps = 35; page.edit(original, false, false, keys);
        QSignalSpy saved(&page, &SettingsPage::saved), cancelled(&page, &SettingsPage::cancelled);
        auto *cancel = page.findChild<QPushButton *>("cancelSettings"); QVERIFY(cancel); cancel->click(); QCOMPARE(cancelled.size(), 1); QCOMPARE(saved.size(), 0);
        auto *fps = page.findChild<QComboBox *>("captureFps");
        QVERIFY(fps);
        QCOMPARE(fps->currentData().toInt(), 60);
        page.edit(original, false, true, keys); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 1);
        const auto options = qvariant_cast<GsrOptions>(saved[0][0]); QCOMPARE(options.bitrateMbps, 35); QCOMPARE(options.source, "DP-1");
        page.edit(original, false, false, keys); fps->setCurrentIndex(fps->findData(144));
        page.setCaptureActive(true); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 2);
        QCOMPARE(qvariant_cast<GsrOptions>(saved[1][0]).frameRate, 60);
        page.edit(original, false, false, keys);
        const auto keyEdits = page.findChildren<QKeySequenceEdit *>(); QCOMPARE(keyEdits.size(), 4);
        keyEdits[1]->setKeySequence(QKeySequence("Alt+Z")); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 2);
    }
    void editorFlushOnlyPublishesEditedScene() {
        QTemporaryDir directory; const auto path = directory.filePath("image with spaces.png");
        QImage image(160, 90, QImage::Format_RGBA8888); image.fill(Qt::red); QVERIFY(image.save(path));
        OverlayEditor editor; QSignalSpy changed(&editor, &OverlayEditor::changed);
        editor.flush(); QCOMPARE(changed.size(), 0);
        OverlayScene scene; scene.enabled = true; editor.edit(scene);
        QLineEdit *pathField = nullptr; for (auto *field : editor.findChildren<QLineEdit *>()) if (field->accessibleName() == "Overlay media path") pathField = field;
        QVERIFY(pathField); pathField->setText(path); button(&editor, "Add local media")->click();
        editor.flush(); QCOMPARE(changed.size(), 1);
        const auto applied = qvariant_cast<OverlayScene>(changed[0][0]); QCOMPARE(applied.layers.size(), 1); QCOMPARE(applied.layers[0].path, path);
        editor.flush(); QCOMPARE(changed.size(), 1);
        const auto picker = editor.findChild<QFileDialog *>(); QVERIFY(picker); QVERIFY(!picker->isWindow());
    }
    void dashboardOverlaySwitchPersistsConfirmedState() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        Controller controller(caps, {}, true);
        auto *dashboard = [&]() -> Dashboard * {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (auto *candidate = qobject_cast<Dashboard *>(widget)) return candidate;
            }
            return nullptr;
        }();
        QVERIFY(dashboard);
        auto *toggle = dashboard->findChild<QPushButton *>("overlayToggle");
        auto *editorButton = dashboard->findChild<QPushButton *>("openOverlays");
        QVERIFY(toggle);
        QVERIFY(editorButton);
        QVERIFY(!toggle->isChecked());
        toggle->click();
        QVERIFY(!toggle->isEnabled());
        QVERIFY(!editorButton->isEnabled());
        QTRY_VERIFY(toggle->isEnabled());
        QVERIFY(editorButton->isEnabled());
        QVERIFY(toggle->isChecked());
        QVERIFY(Settings().scene().enabled);
        toggle->click();
        QTRY_VERIFY(toggle->isEnabled());
        QVERIFY(!toggle->isChecked());
        QVERIFY(!Settings().scene().enabled);
    }
    void inactiveControllerRejectsMissingMedia() {
        QTemporaryDir directory; QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        OverlayScene original; original.enabled = true; OverlayLayer layer; layer.path = directory.filePath("missing.png"); original.layers.append(layer);
        GsrOptions options; options.source = "DP-1"; options.outputDirectory = directory.path(); QString error;
        Settings settings; QVERIFY(settings.save(options, original, false, &error));
        Controller controller(caps, {}, true);
        Dashboard *dashboard = nullptr; for (auto *widget : QApplication::topLevelWidgets()) if (widget->objectName() == "dashboard") dashboard = qobject_cast<Dashboard *>(widget);
        QVERIFY(dashboard); auto *editor = dashboard->overlayEditor(); editor->edit(original);
        auto *opacity = editor->findChild<QSpinBox *>(); QVERIFY(opacity); opacity->setValue(23); editor->flush();
        auto failed = [&] { for (auto *label : editor->findChildren<QLabel *>()) if (label->text().contains("Cannot read media")) return true; return false; };
        QTRY_VERIFY(failed());
        QCOMPARE(Settings().scene().layers[0].opacity, 100);
    }
    void controllerReportsPersistenceFailure() {
        QTemporaryDir directory; const auto blocked = directory.filePath("regular-file");
        QFile file(blocked); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("blocked"); file.close();
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, blocked);
        Controller controller(caps, {}, true);
        Dashboard *dashboard = nullptr; for (auto *widget : QApplication::topLevelWidgets()) if (widget->objectName() == "dashboard") dashboard = qobject_cast<Dashboard *>(widget);
        QVERIFY(dashboard); auto *editor = dashboard->overlayEditor(); editor->edit({});
        QCheckBox *master = nullptr; for (auto *check : editor->findChildren<QCheckBox *>()) if (check->text() == "Enable composition") master = check;
        QVERIFY(master); master->setChecked(true); editor->flush();
        auto failed = [&] { for (auto *label : editor->findChildren<QLabel *>()) if (label->text().contains("not saved")) return true; return false; };
        QTRY_VERIFY(failed());
        for (auto *label : editor->findChildren<QLabel *>()) QVERIFY(label->text() != "Layers applied and saved.");
    }
};
QTEST_MAIN(UiTests)
#include "tst_ui.moc"
