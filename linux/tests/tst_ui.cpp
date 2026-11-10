#include "../ui/dashboard.h"
#include "../ui/library.h"
#include "../ui/settings_page.h"
#include "../ui/overlay_editor.h"
#include "../app/controller.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QApplication>
#include <QLabel>
#include <QSpinBox>
#include <QWheelEvent>
#include <algorithm>

using namespace Shadowless;
Q_DECLARE_METATYPE(Shadowless::GsrOptions)
Q_DECLARE_METATYPE(Shadowless::OverlayScene)

class UiTests : public QObject {
    Q_OBJECT
    const GsrCapabilities caps{"6.1.5", "nvidia", "wayland", {"h264", "hevc"}, {"DP-1"}};
    const QStringList keys{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"};
    QPushButton *button(QWidget *parent, const QString &text) {
        for (auto *candidate : parent->findChildren<QPushButton *>()) if (candidate->text() == text) return candidate;
        return nullptr;
    }
private slots:
    void initTestCase() { qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/dev/null"); }
    void showsRecordingAlongsideReplay() {
        Dashboard dashboard(caps);
        dashboard.show();
        dashboard.updateState(GsrOptions{}, {}, true, true, false, false);
        const auto recording = dashboard.findChild<QPushButton *>("replayRecording");
        QVERIFY(recording && recording->isVisible() && recording->isEnabled());
        QVERIFY(dashboard.findChild<QPushButton *>("captureAction")->isEnabled());
    }
    void exposesCaptureDeviceSelectors() {
        SettingsPage page(caps);
        const auto output = page.findChild<QComboBox *>("audioOutput");
        const auto input = page.findChild<QComboBox *>("audioInput");
        QVERIFY(output && input);
        QCOMPARE(output->currentText(), "Default output");
        QCOMPARE(input->currentText(), "Default input");
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
        QCOMPARE(settings.accent(), "mint");

        SettingsPage page(caps);
        page.edit(options, false, false, keys, settings.accent());
        const auto selector = page.findChild<QComboBox *>("accentColor");
        QVERIFY(selector);
        QCOMPARE(selector->count(), 5);
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
        GsrOptions options;
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
        QVERIFY(dashboard.findChild<QPushButton *>("captureToggle")->isEnabled());
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
    void dashboardOffersTheActionForTheSelectedMode() {
        Dashboard dashboard(caps);
        dashboard.show();
        GsrOptions options;
        options.source = "DP-1";
        options.bufferSeconds = 4;
        auto *action = dashboard.findChild<QPushButton *>("captureAction");
        QVERIFY(action);
        QSignalSpy saved(&dashboard, &Dashboard::saveReplay);
        QSignalSpy toggled(&dashboard, &Dashboard::toggleCapture);
        dashboard.updateState(options, {}, false, false, false, false);
        auto *toggle = dashboard.findChild<QPushButton *>("captureToggle");
        QVERIFY(toggle);
        toggle->click();
        QCOMPARE(toggled.size(), 1);
        QVERIFY(!toggle->isChecked());
        toggled.clear();
        QCOMPARE(action->text(), "Start instant replay");
        action->click();
        QCOMPARE(toggled.size(), 1);
        dashboard.updateState(options, {}, true, true, false, false);
        QCOMPARE(action->text(), "Save 4 sec");
        action->click();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().first().toInt(), 4);
        options.bufferSeconds = 30;
        dashboard.updateState(options, {}, true, true, false, false);
        QCOMPARE(action->text(), "Save 10 sec");
        options.continuousRecording = true;
        dashboard.updateState(options, {}, true, true, false, false);
        QCOMPARE(action->text(), "Stop recording");
        action->click();
        QCOMPARE(toggled.size(), 2);
        QCOMPARE(saved.size(), 1);
        dashboard.updateState(options, {}, true, true, false, true);
        QVERIFY(!action->isEnabled());
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
        const auto keyEdits = page.findChildren<QKeySequenceEdit *>(); QCOMPARE(keyEdits.size(), 3);
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
