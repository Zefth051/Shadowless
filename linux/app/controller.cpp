#include "controller.h"
#include "../ui/dashboard.h"
#include "../ui/settings_page.h"
#include "../ui/theme.h"
#include "../ui/overlay_editor.h"
#include "../platform/hyprland/overlay_surface.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QtConcurrentRun>

namespace Shadowless {
Controller::Controller(const GsrCapabilities &caps, const QString &probeError, bool background)
    : m_caps(caps), m_options(m_settings.capture(caps)), m_scene(m_settings.scene()),
      m_menu(std::make_unique<QMenu>()), m_dashboard(std::make_unique<Dashboard>(caps)), m_indicator(std::make_unique<QLabel>()) {
    m_indicatorEnabled = m_settings.indicator();
    m_shortcutKeys = m_settings.shortcutKeys(); m_shortcuts.setKeys(m_shortcutKeys);
    m_dashboard->setShortcutKeys(m_shortcutKeys);
    m_plugin = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../plugins/libshadowless-overlay.so");
    m_dashboard->notice(probeError); OverlaySurface::prepare(m_dashboard.get());
    m_indicator->setFixedSize(180, 36); m_indicator->setAlignment(Qt::AlignCenter);
    setAccent(m_settings.accent());
    OverlaySurface::prepare(m_indicator.get(), true);
    m_tray.setToolTip("Shadowless");
    m_menu->addAction("Open Shadowless", this, &Controller::show);
    m_menu->addAction("Save replay", this, [this] { saveReplay(qMin(10, m_options.bufferSeconds)); });
    m_menu->addAction("Start / stop capture", this, &Controller::toggleCapture);
    m_recordingAction = m_menu->addAction("Start recording", this, &Controller::toggleReplayRecording);
    m_menu->addSeparator(); m_menu->addAction("Quit", this, &Controller::quit); m_tray.setContextMenu(m_menu.get()); m_tray.show();
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) { if (reason == QSystemTrayIcon::Trigger) command("toggle"); });
    connect(m_dashboard.get(), &Dashboard::hideRequested, this, &Controller::hide);
    connect(m_dashboard.get(), &Dashboard::toggleCapture, this, &Controller::toggleCapture);
    connect(m_dashboard.get(), &Dashboard::saveReplay, this, &Controller::saveReplay);
    connect(m_dashboard.get(), &Dashboard::toggleReplayRecording, this, &Controller::toggleReplayRecording);
    connect(m_dashboard.get(), &Dashboard::recordingMode, this, [this](bool recording) { if (m_engine.active()) return; m_options.continuousRecording = recording; persist(); refresh(); });
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
    connect(m_dashboard.get(), &Dashboard::settingsRequested, this, [this] { m_dashboard->settingsPage()->edit(m_options, m_indicatorEnabled, m_engine.active(), m_shortcutKeys, m_accent); });
    connect(m_dashboard.get(), &Dashboard::overlayEnabled, this, [this](bool enabled) {
        auto scene = m_scene;
        scene.enabled = enabled;
        apply(scene);
    });
    connect(m_dashboard.get(), &Dashboard::editorRequested, this, [this] { m_dashboard->overlayEditor()->edit(m_scene); });
    connect(m_dashboard->settingsPage(), &SettingsPage::saved, this, [this](const GsrOptions &options, bool indicator, const QStringList &keys, const QString &accent) {
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
    connect(&m_engine, &GsrCapture::error, this, [this](const QString &message) { m_saving = false; m_dashboard->notice(message); refresh(); });
    connect(&m_engine, &GsrCapture::recordingChanged, this, [this] { refresh(); });
    connect(&m_engine, &GsrCapture::clipSaved, this, [this](const QString &path) { m_saving = false; m_dashboard->notice("Saved " + QFileInfo(path).fileName()); m_dashboard->refreshLibrary(m_options.outputDirectory); refresh(); });
    connect(&m_overlays, &OverlayClient::finished, this, &Controller::finishApply);
    connect(&m_validation, &QFutureWatcher<QString>::finished, this, [this] {
        m_validating = false; const auto error = m_validation.result(); finishApply(error.isEmpty(), error);
    });
    refresh(); if (!background) show(); m_shortcuts.start();
}
Controller::~Controller() = default;
void Controller::show() { if (!m_quitting) { m_dashboard->refreshLibrary(m_options.outputDirectory); OverlaySurface::show(m_dashboard.get()); } }
void Controller::hide() { m_dashboard->overlayEditor()->flush(); m_dashboard->hide(); }
void Controller::command(const QString &command) {
    if (m_quitting) return;
    if (command == "toggle") { if (m_dashboard->isVisible()) hide(); else show(); }
    else if (command == "show") show(); else if (command == "hide") hide();
    else if (command == "save") saveReplay(qMin(10, m_options.bufferSeconds));
    else if (command == "capture") toggleCapture(); else if (command == "quit") quit();
}
void Controller::refresh() {
    m_dashboard->setOverlayBusy(m_validating || m_overlays.busy() || m_pending.has_value() || m_stopping);
    m_dashboard->settingsPage()->setCaptureActive(m_engine.active());
    m_dashboard->updateState(m_options, m_scene, m_engine.active(), m_engine.running(), m_saving, m_stopping,
                             m_engine.replayRecording(), m_engine.busy());
    const bool recording = m_options.continuousRecording || m_engine.replayRecording();
    m_recordingAction->setVisible(m_engine.running() && !m_options.continuousRecording);
    m_recordingAction->setEnabled(!m_engine.busy() && !m_stopping);
    m_recordingAction->setText(m_engine.replayRecording() ? "Stop recording" : "Start recording");
    m_tray.setToolTip(m_engine.running() ? (recording ? "Shadowless · Recording" : "Shadowless · Replay buffering") : "Shadowless · Capture off");
    if (!m_quitting && m_indicatorEnabled && m_engine.running()) {
        m_indicator->setText(m_saving ? "●  Saving clip…" : recording ? "●  Recording" : "◉  Replay buffering");
        if (!m_indicator->isVisible()) OverlaySurface::show(m_indicator.get());
    } else m_indicator->hide();
}
void Controller::toggleCapture() {
    if (m_quitting || m_stopping) return;
    if (m_engine.active()) { m_stopping = true; m_engine.stop(); }
    else {
        if (m_validating || m_overlays.busy()) { m_dashboard->notice("Wait for the pending overlay update before starting capture."); return; }
        if (!QFileInfo::exists(m_plugin)) { m_dashboard->notice("The Shadowless composition plugin is missing. Build the plugins target first."); return; }
        auto options = m_options; options.overlayPlugin = m_plugin; m_engine.start(options, m_caps);
    }
    refresh();
}
void Controller::saveReplay(int seconds) {
    if (!m_engine.running() || m_engine.busy() || m_saving || m_stopping || m_options.continuousRecording) return;
    m_saving = true; m_engine.saveReplay(seconds); refresh();
}
void Controller::toggleReplayRecording() {
    if (!m_engine.running() || m_engine.busy() || m_stopping || m_options.continuousRecording) return;
    if (m_engine.replayRecording()) m_engine.stopReplayRecording();
    else m_engine.startReplayRecording();
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
    m_indicator->setStyleSheet(QString(
        "background:rgba(11,13,15,220);color:%1;border:1px solid #39444a;border-radius:8px;font:12px sans-serif;"
    ).arg(accentColor(name).name()));
}

bool Controller::persist(QString *result) {
    QString error; const bool ok = m_settings.save(m_options, m_scene, m_indicatorEnabled, &error, m_shortcutKeys, m_accent);
    if (!ok) m_dashboard->notice(error);
    if (result) *result = error;
    return ok;
}
void Controller::quit() {
    if (m_quitting) return;
    m_dashboard->overlayEditor()->flush();
    if (m_saveLevels.isActive()) { m_saveLevels.stop(); persist(); }
    m_quitting = true; m_dashboard->hide(); m_indicator->hide();
    if (m_engine.active()) { m_stopping = true; m_engine.stop(); }
    else { drainPending(); finishQuitIfReady(); }
}
void Controller::finishQuitIfReady() {
    if (m_quitting && !m_engine.active() && !m_overlays.busy() && !m_validating && !m_pending) QCoreApplication::quit();
}
} // namespace Shadowless
