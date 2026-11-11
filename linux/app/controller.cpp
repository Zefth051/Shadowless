#include "controller.h"
#include "../ui/dashboard.h"
#include "../ui/settings_page.h"
#include "../ui/theme.h"
#include "../ui/overlay_editor.h"
#include "../platform/hyprland/overlay_surface.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include "../ui/status_indicator.h"
#include "../ui/clip_notification.h"
#include "../capture/replay_paths.h"
#include <QMenu>
#include <QMessageBox>
#include <QtConcurrentRun>

namespace Shadowless {
Controller::Controller(const GsrCapabilities &caps, const QString &probeError, bool background)
    : m_caps(caps), m_options(m_settings.capture(caps)), m_scene(m_settings.scene()),
      m_menu(std::make_unique<QMenu>()), m_dashboard(std::make_unique<Dashboard>(caps)), m_indicator(std::make_unique<StatusIndicator>()),
      m_notification(std::make_unique<ClipNotification>()) {
    m_savedSound.setSource(QUrl("qrc:/shadowless/replay-saved.wav"));
    m_savedSound.setLoopCount(1);
    connect(&m_savedSound, &QSoundEffect::statusChanged, this, [this] {
        if (m_savedSound.status() == QSoundEffect::Error) qWarning() << "Replay saved sound is unavailable.";
    });
    connect(&m_gameQuery, &QFutureWatcher<QString>::finished, this, [this] {
        auto action = std::move(m_afterGame); m_afterGame = {};
        if (!m_quitting && action) action();
        finishQuitIfReady();
    });
    m_indicatorEnabled = m_settings.indicator();
    m_indicatorPosition = m_settings.indicatorPosition();
    m_shortcutKeys = m_settings.shortcutKeys(); m_shortcuts.setKeys(m_shortcutKeys);
    m_dashboard->setShortcutKeys(m_shortcutKeys);
    m_plugin = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../plugins/libshadowless-overlay.so");
    m_dashboard->notice(probeError); OverlaySurface::prepare(m_dashboard.get());
    setAccent(m_settings.accent());
    OverlaySurface::prepare(m_indicator.get(), true);
    OverlaySurface::setPosition(m_indicator.get(), m_indicatorPosition);
    m_tray.setToolTip("Shadowless");
    m_menu->addAction("Open Shadowless", this, &Controller::show);
    m_menu->addAction("Save replay", this, &Controller::saveReplay);
    m_menu->addAction("Start / stop capture", this, &Controller::toggleCapture);
    m_recordingAction = m_menu->addAction("Start recording", this, &Controller::toggleReplayRecording);
    m_menu->addAction("Check for updates", this, [this] { m_updates.check(true); });
    m_menu->addAction("About Shadowless", this, [this] {
        QMessageBox::about(m_dashboard.get(), "About Shadowless", "Shadowless " + QCoreApplication::applicationVersion()
            + "\nDeveloped with AI assistance.\nBased on Captail by FaulMit (GPL-2.0-or-later).\nhttps://github.com/Zefth051/Shadowless");
    });
    connect(&m_updates, &UpdateService::status, this, [this](const QString &message) {
        m_dashboard->notice(message);
        m_tray.showMessage("Shadowless updates", message);
    });
    connect(&m_updates, &UpdateService::available, this, [this](const LinuxRelease &release) {
        if (m_quitting) return;
        if (QMessageBox::question(m_dashboard.get(), "Shadowless update",
            "Shadowless " + release.version + " is available. Download and install it for the next launch? Recording will continue.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) m_updates.download(release);
    });
    if (UpdateService::enabled()) QTimer::singleShot(10000, &m_updates, [this] { m_updates.check(false); });
    m_menu->addSeparator(); m_menu->addAction("Quit", this, &Controller::quit); m_tray.setContextMenu(m_menu.get()); m_tray.show();
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) { if (reason == QSystemTrayIcon::Trigger) command("toggle"); });
    connect(m_dashboard.get(), &Dashboard::hideRequested, this, &Controller::hide);
    connect(m_dashboard.get(), &Dashboard::saveReplay, this, &Controller::saveReplay);
    connect(m_dashboard.get(), &Dashboard::recordingMode, this, [this](bool recording) {
        if (m_quitting || m_stopping || m_saving || m_engine.busy() || m_afterGame) return;
        if (m_engine.active()) {
            if (!m_engine.running()) {
                if (recording == m_options.continuousRecording) toggleCapture();
                return;
            }
            if (recording && !m_options.continuousRecording) toggleReplayRecording();
            else if (!recording && m_options.continuousRecording) return;
            else toggleCapture();
        } else {
            m_options.continuousRecording = recording;
            persist(); toggleCapture();
        }
    });
    connect(m_dashboard.get(), &Dashboard::audioChanged, this, [this](bool microphone, bool enabled) {
        bool &captured = microphone ? m_options.microphone : m_options.systemAudio;
        bool &muted = microphone ? m_options.levels.microphoneMuted : m_options.levels.systemMuted;
        if (captured) muted = !enabled;
        else if (!m_engine.active() && enabled) { captured = true; muted = false; }
        updateAudioLevels();
    });
    m_saveLevels.setSingleShot(true);
    m_saveLevels.setInterval(160);
    connect(&m_saveLevels, &QTimer::timeout, this, [this] { persist(); });
    connect(m_dashboard.get(), &Dashboard::volumeChanged, this, [this](bool microphone, int volume) {
        if (microphone) m_options.levels.microphoneVolume = volume;
        else m_options.levels.systemVolume = volume;
        updateAudioLevels();
    });
    connect(m_dashboard.get(), &Dashboard::settingsRequested, this, [this] { m_dashboard->settingsPage()->edit(m_options, m_indicatorEnabled, m_engine.active(), m_shortcutKeys, m_accent, m_indicatorPosition); });
    connect(m_dashboard.get(), &Dashboard::overlayEnabled, this, [this](bool enabled) {
        auto scene = m_scene;
        scene.enabled = enabled;
        apply(scene);
    });
    connect(m_dashboard.get(), &Dashboard::editorRequested, this, [this] { m_dashboard->overlayEditor()->edit(m_scene); });
    connect(m_dashboard->settingsPage(), &SettingsPage::saved, this, [this](const GsrOptions &options, bool indicator, const QStringList &keys, const QString &accent, const QString &position) {
        m_indicatorPosition = position;
        OverlaySurface::setPosition(m_indicator.get(), position);
        if (m_accent != accent) setAccent(accent);
        if (!m_engine.active()) m_options = options;
        m_indicatorEnabled = indicator; m_shortcutKeys = keys; m_shortcuts.setKeys(keys); m_dashboard->setShortcutKeys(keys); persist(); refresh(); m_dashboard->home(); m_dashboard->refreshLibrary(m_options.outputDirectory);
    });
    connect(m_dashboard->overlayEditor(), &OverlayEditor::changed, this, &Controller::apply);
    connect(&m_shortcuts, &Shortcuts::activated, this, [this](const QString &action) {
        if (action == "toggle-overlay") command("toggle"); else if (action == "save-replay") command("save"); else if (action == "toggle-capture") toggleCapture();
    });
    connect(&m_shortcuts, &Shortcuts::status, m_dashboard.get(), &Dashboard::shortcutStatus);
    connect(&m_engine, &GsrCapture::started, this, [this] {
        refresh(); const auto scene = m_pending.value_or(m_scene); m_pending.reset(); apply(scene);
    });
    connect(&m_engine, &GsrCapture::diagnostic, this, [](const QString &message) {
        if (message.startsWith("Shadowless overlay metrics:")) qInfo().noquote() << message;
    });
    connect(&m_engine, &GsrCapture::stopped, this, [this] {
        m_stopping = false; m_saving = false; refresh();
        m_dashboard->refreshLibrary(m_options.outputDirectory);
        drainPending(); finishQuitIfReady();
    });
    connect(&m_engine, &GsrCapture::error, this, [this](const QString &message) {
        if ((m_saving || m_recordingSave) && !m_quitting) m_notification->present("Clip could not be saved", message, 3500, ClipNotification::Failed);
        m_saving = false; m_recordingSave = false; m_dashboard->notice(message); refresh();
    });
    connect(&m_engine, &GsrCapture::recordingChanged, this, [this] { refresh(); });
    connect(&m_engine, &GsrCapture::clipSaved, this, [this](const QString &path, bool recording) {
        QString error;
        const auto saved = m_options.organizeByGame ? routeClip(m_options.outputDirectory, path,
            recording ? m_recordingGame : m_clipGame, &error) : path;
        if (recording) m_recordingSave = false;
        else m_saving = false;
        m_indicator->saved();
        if (!m_quitting) {
            m_savedSound.play();
            m_notification->present(error.isEmpty() ? "Clip saved" : "Clip saved in main folder", error.isEmpty() ? QFileInfo(saved).fileName() : error);
        }
        m_dashboard->notice(error.isEmpty() ? "Saved " + QFileInfo(saved).fileName() : error);
        m_dashboard->refreshLibrary(m_options.outputDirectory); refresh();
    });
    connect(&m_overlays, &OverlayClient::finished, this, &Controller::finishApply);
    connect(&m_validation, &QFutureWatcher<QString>::finished, this, [this] {
        m_validating = false; const auto error = m_validation.result(); finishApply(error.isEmpty(), error);
    });
    refresh(); if (!background) show(); m_shortcuts.start();
}
Controller::~Controller() = default;
void Controller::show() { if (!m_quitting) { m_dashboard->refreshLibrary(m_options.outputDirectory); OverlaySurface::show(m_dashboard.get()); } }
void Controller::hide() {
    m_dashboard->overlayEditor()->flush();
    m_dashboard->hide();
    m_dashboard->home();
}
void Controller::command(const QString &command) {
    if (m_quitting) return;
    if (command == "toggle") { if (m_dashboard->isVisible()) hide(); else show(); }
    else if (command == "show") show(); else if (command == "hide") hide();
    else if (command == "save") saveReplay();
    else if (command == "capture") toggleCapture(); else if (command == "quit") quit();
}
void Controller::refresh() {
    m_dashboard->overlayEditor()->setPreviewSocket(m_engine.running() && !m_stopping ? m_engine.overlaySocketPath() + ".preview" : QString());
    m_dashboard->setOverlayBusy(m_validating || m_overlays.busy() || m_pending.has_value() || m_stopping);
    m_dashboard->settingsPage()->setCaptureActive(m_engine.active() || bool(m_afterGame));
    m_dashboard->updateState(m_options, m_scene, m_engine.active(), m_engine.running(), m_saving, m_stopping,
                             m_engine.replayRecording(), m_engine.busy());
    const bool recording = m_options.continuousRecording || m_engine.replayRecording();
    m_recordingAction->setVisible(m_engine.running() && !m_options.continuousRecording);
    m_recordingAction->setEnabled(!m_engine.busy() && !m_stopping);
    m_recordingAction->setText(m_engine.replayRecording() ? "Stop recording" : "Start recording");
    m_tray.setToolTip(m_engine.running() ? (recording ? "Shadowless · Recording" : "Shadowless · Replay buffering") : "Shadowless · Capture off");
    if (!m_quitting && m_indicatorEnabled && m_engine.running()) {
        m_indicator->setState(m_saving ? StatusIndicator::Saving : recording ? StatusIndicator::Recording : StatusIndicator::Buffering);
        if (!m_indicator->isVisible()) OverlaySurface::show(m_indicator.get());
    } else m_indicator->hide();
}
void Controller::toggleCapture() {
    if (m_quitting || m_stopping || m_afterGame) return;
    if (m_engine.active()) {
        if (m_options.continuousRecording || m_engine.replayRecording()) {
            m_recordingSave = true;
            m_notification->present("Saving recording…", {}, 30000, ClipNotification::Saving);
        }
        m_stopping = true; m_engine.stop();
    }
    else {
        if (m_validating || m_overlays.busy()) { m_dashboard->notice("Wait for the pending overlay update before starting capture."); return; }
        if (!QFileInfo::exists(m_plugin)) { m_dashboard->notice("The Shadowless composition plugin is missing. Build the plugins target first."); return; }
        identifyGame([this](const QString &game) {
            m_recordingGame = game;
            auto options = m_options; options.overlayPlugin = m_plugin; m_engine.start(options, m_caps); refresh();
        });
    }
    refresh();
}
void Controller::saveReplay() {
    if (!m_engine.running() || m_engine.busy() || m_afterGame || m_saving || m_stopping || m_options.continuousRecording) return;
    m_saving = true;
    m_notification->present("Saving clip…", {}, 30000, ClipNotification::Saving);
    identifyGame([this](const QString &game) {
        m_clipGame = game;
        if (!m_engine.running() || m_stopping) { m_saving = false; m_notification->dismiss(); refresh(); return; }
        m_engine.saveReplay(m_options.bufferSeconds); refresh();
    });
    refresh();
}
void Controller::identifyGame(std::function<void(const QString &)> action) {
    if (!m_options.organizeByGame) { action({}); return; }
    m_afterGame = [this, action = std::move(action)] { action(m_gameQuery.result()); };
    m_gameQuery.setFuture(QtConcurrent::run(&currentGame));
}
void Controller::toggleReplayRecording() {
    if (!m_engine.running() || m_engine.busy() || m_afterGame || m_saving || m_stopping || m_options.continuousRecording) return;
    if (m_engine.replayRecording()) {
        m_recordingSave = true; m_notification->present("Saving recording…", {}, 30000, ClipNotification::Saving); m_engine.stopReplayRecording();
    } else identifyGame([this](const QString &game) { m_recordingGame = game; m_engine.startReplayRecording(); refresh(); });
    refresh();
}
void Controller::updateAudioLevels() {
    m_engine.setAudioLevels(m_options.levels);
    m_saveLevels.start();
    refresh();
}
void Controller::apply(const OverlayScene &scene) {
    if (m_overlays.busy() || m_validating) { m_pending = scene; return; }
    m_dashboard->setOverlayBusy(true);
    m_pending.reset(); m_applying = scene;
    if (!m_engine.active()) {
        m_validating = true;
        m_validation.setFuture(QtConcurrent::run([scene] {
            OverlayScene checked; QString error;
            if (!OverlayScene::fromJson(scene.toJson(), &checked, &error)) return error;
            if (checked.enabled) for (const auto &layer : checked.layers) {
                if (!layer.enabled || layer.opacity == 0) continue;
                Media media; if (!media.open(layer.path, QSize(640, 360), &error)) return error;
            }
            return QString{};
        })); return;
    }
    if (!m_engine.running() || m_stopping) { m_pending = scene; return; }
    m_overlays.apply(m_engine.overlaySocketPath(), scene);
}
void Controller::finishApply(bool success, const QString &message) {
    QString error = message;
    if (success) {
        m_scene = m_applying;
        if (!persist(&error)) { success = false; error = "Layers updated in memory, but settings were not saved. " + error; }
    }
    m_dashboard->overlayEditor()->setResult(success, error);
    if (!success) m_dashboard->notice("Capture overlays: " + error);
    refresh(); drainPending(); finishQuitIfReady();
}
void Controller::drainPending() {
    if (!m_pending || m_overlays.busy() || m_validating || m_stopping) return;
    const auto scene = *m_pending; m_pending.reset(); apply(scene);
}
void Controller::setAccent(const QString &name) {
    m_accent = name;
    m_dashboard->setAccent(name);
    m_tray.setIcon(accentIcon(name));
    applyTheme(m_indicator.get(), name);
    m_notification->setAccent(accentColor(name));
}

bool Controller::persist(QString *result) {
    QString error; const bool ok = m_settings.save(m_options, m_scene, m_indicatorEnabled, &error, m_shortcutKeys, m_accent, m_indicatorPosition);
    if (!ok) m_dashboard->notice(error);
    if (result) *result = error;
    return ok;
}
void Controller::quit() {
    if (m_quitting) return;
    m_dashboard->overlayEditor()->flush();
    if (m_saveLevels.isActive()) { m_saveLevels.stop(); persist(); }
    m_quitting = true; m_dashboard->hide(); m_indicator->hide();
    m_notification->dismiss();
    if (m_options.continuousRecording || m_engine.replayRecording()) m_recordingSave = true;
    if (m_engine.active()) { m_stopping = true; m_engine.stop(); }
    else { drainPending(); finishQuitIfReady(); }
}
void Controller::finishQuitIfReady() {
    if (m_quitting && !m_engine.active() && !m_overlays.busy() && !m_validating && !m_pending && !m_afterGame) QCoreApplication::quit();
}
} // namespace Shadowless
