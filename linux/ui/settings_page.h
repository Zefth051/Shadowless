#pragma once
#include "../capture/audio.h"
#include <QWidget>
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
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(const GsrCapabilities &caps, QWidget *parent = nullptr);
    void edit(const GsrOptions &options, bool indicator, bool active, const QStringList &keys, const QString &accent = "violet", const QString &position = "top-right", bool launchAtLogin = false, bool startReplayOnLaunch = false);
    bool launchAtLogin() const;
    bool startReplayOnLaunch() const;
    void showError(const QString &error);
    void showVideo();
    void showStorage();
    void showAudio();
    void setShortcutStatus(const QString &message);
    void setCaptureActive(bool active);
signals:
    void saved(const Shadowless::GsrOptions &options, bool indicator, const QStringList &keys, const QString &accent, const QString &position);
    void cancelled();
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
    QLineEdit *m_folder;
    QCheckBox *m_organizeByGame;
    QCheckBox *m_launchAtLogin, *m_startReplayOnLaunch;
    QLabel *m_error, *m_shortcuts, *m_activeNotice;
    QList<QKeySequenceEdit *> m_keys;
    QTableWidget *m_applications;
    QPushButton *m_refreshAudio;
    QLabel *m_audioStatus;
    AudioCatalog m_audioCatalog;
    QFutureWatcher<AudioCatalog> m_audioProbe;
    void focusSection(QWidget *field);
    void refreshAudio();
    void populateAudio(const QString &output, const QString &input, const QMap<QString, int> &tracks);
    QMap<QString, int> applicationTracks() const;
};
} // namespace Shadowless
