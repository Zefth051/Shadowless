#pragma once
#include "../settings/settings.h"
#include "../capture/audio.h"
#include "../overlays/input/core/input_source.h"
#include <QWidget>
#include <QFutureWatcher>
class QCheckBox;
class QComboBox;
class QSpinBox;
class QScrollArea;
class QStackedWidget;
class QLineEdit;
class QLabel;
class QKeySequenceEdit;
class QTableWidget;
class QPushButton;
namespace Shadowless {
class InputOverlayPreview;
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(const GsrCapabilities &caps, QWidget *parent = nullptr);
    void edit(const GsrOptions &options, bool indicator, bool active, const QStringList &keys, const QString &accent = "violet", const QString &position = "top-right", bool launchAtLogin = false, bool startReplayOnLaunch = false);
    bool launchAtLogin() const;
    bool startReplayOnLaunch() const;
    bool discordPresence() const;
    void setDiscordPresence(bool enabled);
    bool idleCompression() const;
    void setIdleCompression(bool enabled);
    InputOverlaySettings inputOverlay() const;
    void setInputOverlay(const InputOverlaySettings &value);
    void showError(const QString &error);
    void showVideo();
    void showStorage();
    void showAudio();
    void setShortcutStatus(const QString &message);
    void setCaptureActive(bool active);
    // Live input preview driven by the controller's input source (never read by the page itself).
    void setInputLive(const Input::InputState &state);
    void setInputStatus(Input::InputStatus status, const QString &detail);
signals:
    void saved(const Shadowless::GsrOptions &options, bool indicator, const QStringList &keys, const QString &accent, const QString &position);
    void cancelled();
    // Layout id while the page is visible with an overlay selected, empty otherwise. The controller reads
    // input only while this is non-empty (or while recording the sidecar).
    void inputPreviewWanted(const QString &layoutId);
private:
    GsrCapabilities m_caps;
    GsrOptions m_original;
    QWidget *m_captureFields;
    QStackedWidget *m_pages;
    QScrollArea *m_scroll;
    QComboBox *m_source, *m_codec, *m_fps, *m_bitrate, *m_container;
    QComboBox *m_audioMode, *m_audioOutput, *m_audioInput, *m_audioCodec, *m_microphoneTrack;
    QSpinBox *m_buffer, *m_customFps, *m_customBitrate, *m_microphoneBoost;
    QComboBox *m_audioBitrate, *m_accent, *m_indicatorPosition, *m_pcTrack;
    QCheckBox *m_system, *m_microphone, *m_separate, *m_indicator;
    QLineEdit *m_folder, *m_customAccent;
    QCheckBox *m_organizeByGame;
    QCheckBox *m_launchAtLogin, *m_startReplayOnLaunch, *m_discordPresence, *m_idleCompression;
    QLabel *m_error, *m_shortcuts, *m_activeNotice;
    InputOverlayPreview *m_inputPreview;
    QComboBox *m_inputLayout, *m_inputCorner;
    QSpinBox *m_inputScale, *m_inputOpacity;
    QCheckBox *m_inputSidecar;
    QLabel *m_inputNotice, *m_inputAccess, *m_inputSetup;
    void updateInputOverlay();
    void announceInputPreview();
    Input::InputStatus m_inputStatus = Input::InputStatus::Stopped;
    QString m_inputStatusDetail;
protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    QList<QKeySequenceEdit *> m_keys;
    QTableWidget *m_applications;
    QPushButton *m_refreshAudio;
    QLabel *m_audioStatus;
    AudioCatalog m_audioCatalog;
    QFutureWatcher<AudioCatalog> m_audioProbe;
    void focusSection(QWidget *field);
    void updateActiveSection();
    QList<QPair<QPushButton *, QWidget *>> m_sectionLinks;
    void refreshAudio();
    void populateAudio(const QString &output, const QString &input, const QMap<QString, int> &tracks);
    QMap<QString, int> applicationTracks() const;
};
} // namespace Shadowless
