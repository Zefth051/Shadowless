#pragma once
#include "../capture/gsr_capture.h"
#include "../overlays/layer.h"
#include <QWidget>
class QBoxLayout;
class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QStackedWidget;
namespace Shadowless {
class Library;
class SettingsPage;
class OverlayEditor;
class Dashboard : public QWidget {
    Q_OBJECT
public:
    void setAccent(const QString &name);
    explicit Dashboard(const GsrCapabilities &caps);
    SettingsPage *settingsPage() const { return m_settings; }
    OverlayEditor *overlayEditor() const { return m_editor; }
    void updateState(const GsrOptions &options, const OverlayScene &scene, bool active, bool ready, bool saving, bool stopping,
                     bool replayRecording = false, bool busy = false);
    void notice(const QString &message);
    void setOverlayBusy(bool busy);
    void setShortcutKeys(const QStringList &keys);
    void shortcutStatus(const QString &message);
    void refreshLibrary(const QString &directory);
    void home();
    void escape();
signals:
    void toggleCapture();
    void saveReplay(int seconds);
    void recordingMode(bool continuous);
    void audioChanged(bool microphone, bool enabled);
    void volumeChanged(bool microphone, int volume);
    void toggleReplayRecording();
    void settingsRequested();
    void editorRequested();
    void overlayEnabled(bool enabled);
    void hideRequested();
protected:
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    QFrame *m_panel;
    QLabel *m_logo;
    QBoxLayout *m_homeLayout;
    QScrollArea *m_controlScroll;
    QStackedWidget *m_pages;
    QWidget *m_home, *m_ring;
    SettingsPage *m_settings;
    OverlayEditor *m_editor;
    Library *m_library;
    QLabel *m_status, *m_detail, *m_disk, *m_notice, *m_footer, *m_layerStatus;
    QProgressBar *m_diskUsage;
    QPushButton *m_toggle, *m_action, *m_replay, *m_record, *m_layers, *m_layerToggle;
    QPushButton *m_systemAudio, *m_microphone, *m_back, *m_settingsButton, *m_folder, *m_openFolder, *m_format;
    QPushButton *m_replayRecording, *m_audioSettings;
    QSlider *m_systemVolume, *m_microphoneVolume;
    QLabel *m_systemPercent, *m_microphonePercent;
    QSpinBox *m_seconds;
    QString m_directory, m_actionText;
    bool m_actionSavesReplay = false;
    int m_preferredSeconds = 10;
    int m_panelWidth = 1034, m_panelHeight = 680;
    void positionPanel();
    void showPage(QWidget *page, int width);
    void updateActionText();
};
} // namespace Shadowless
