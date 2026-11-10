#pragma once
#include "../capture/gsr_capture.h"
#include "../settings/settings.h"
#include "../overlays/overlay_client.h"
#include "../platform/hyprland/shortcuts.h"
#include <QSystemTrayIcon>
#include <optional>
#include <QFutureWatcher>
class QLabel;
class QMenu;
class QAction;
namespace Shadowless {
class Dashboard;
class Controller : public QObject {
    Q_OBJECT
public:
    Controller(const GsrCapabilities &caps, const QString &probeError, bool background);
    ~Controller() override;
    void command(const QString &command);
private:
    Settings m_settings;
    GsrCapabilities m_caps;
    GsrOptions m_options;
    OverlayScene m_scene, m_applying;
    std::optional<OverlayScene> m_pending;
    GsrCapture m_engine;
    OverlayClient m_overlays;
    Shortcuts m_shortcuts;
    QSystemTrayIcon m_tray;
    std::unique_ptr<QMenu> m_menu;
    QAction *m_recordingAction;
    std::unique_ptr<Dashboard> m_dashboard;
    std::unique_ptr<QLabel> m_indicator;
    bool m_indicatorEnabled = false, m_saving = false, m_stopping = false, m_quitting = false;
    QString m_plugin, m_accent;
    QStringList m_shortcutKeys;
    QFutureWatcher<QString> m_validation;
    QTimer m_saveLevels;
    bool m_validating = false;
    void refresh();
    void setAccent(const QString &name);
    void show();
    void hide();
    void toggleCapture();
    void saveReplay(int seconds);
    void toggleReplayRecording();
    void updateAudioLevels();
    void apply(const OverlayScene &scene);
    bool persist(QString *error = nullptr);
    void finishApply(bool success, const QString &error);
    void drainPending();
    void finishQuitIfReady();
    void quit();
};
} // namespace Shadowless
