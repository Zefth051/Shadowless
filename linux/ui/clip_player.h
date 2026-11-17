#pragma once
#include <QMap>
#include <QHash>
#include <QPixmap>
#include <QTimer>
#include <QWidget>
#include <QPointer>
class QLabel;
class QSlider;
class QPushButton;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QListWidget;
class QStackedWidget;
class QVBoxLayout;
class QGridLayout;
class QScrollArea;
namespace Shadowless {
class MpvPlayer;
class MediaTools;
class ClipTimeline;
class ClipAudioTracks;
class ClipPlayer : public QWidget {
    Q_OBJECT
public:
    explicit ClipPlayer(QWidget *parent = nullptr);
    void open(const QString &path);
    void setClips(const QStringList &paths, const QString &directory = {});
    void setThumbnail(const QString &path, const QPixmap &image);
    bool fullscreen() const { return m_fullscreen; }
    bool editing() const { return m_editingEnabled; }
    void setFullscreen(bool enabled);
    bool handleEscape();
signals:
    void back();
    void fullscreenChanged(bool enabled);
    void editingChanged(bool enabled);
    void clipExported(const QString &path);
    void exportingChanged(bool active);
protected:
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    MpvPlayer *m_player;
    MediaTools *m_tools;
    QPointer<MediaTools> m_metadataTools;
    QLabel *m_title, *m_error, *m_time, *m_jobStatus, *m_recentEmpty;
    QSlider *m_seek, *m_volume;
    QPushButton *m_play, *m_next, *m_previous, *m_fullscreenButton, *m_edit, *m_export, *m_cancel;
    QComboBox *m_speed, *m_gameFilter, *m_modeFilter;
    QPushButton *m_mute; QCheckBox *m_mix;
    QDoubleSpinBox *m_in, *m_out;
    QListWidget *m_recent;
    QStackedWidget *m_pages;
    QWidget *m_header, *m_controls, *m_editing, *m_recentPanel;
    ClipAudioTracks *m_audio;
    QWidget *m_video;
    void layoutVideo();
    QScrollArea *m_audioScroll;
    ClipTimeline *m_timeline;
    QStringList m_clips;
    QStringList m_audioTitles;
    QStringList m_visibleClips;
    QString m_clipDirectory;
    QHash<QString, QPixmap> m_thumbnails;
    QString m_highlightedSource;
    QTimer m_idle;
    bool m_fullscreen = false, m_analyzePending = false, m_exporting = false;
    bool m_editingEnabled = false, m_metadataReady = false;
    bool m_resumeAfterScrub = false;
    void updateState();
    void filterClips();
    void updateTracks();
    void navigate(int delta);
    void showControls();
    void analyze();
    void chooseExport();
};
}
