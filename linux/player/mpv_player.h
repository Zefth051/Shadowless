#pragma once
#include <QImage>
#include <QWidget>
#include <mpv/client.h>
#include <mpv/render.h>
#include <atomic>

class QOpenGLWidget;
namespace Shadowless {
struct PlaybackTrack { int id; QString title; };
class MpvSurface;
class MpvPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MpvPlayer(QWidget *parent = nullptr);
    ~MpvPlayer() override;
    void open(const QString &path);
    void stop();
    void release();
    void pause(bool paused);
    void seek(qint64 ms);
    void setPlaybackRange(qint64 inMs, qint64 outMs);
    void setVolume(int volume);
    void setMuted(bool muted);
    void setSpeed(double speed);
    void setAudioTracks(const QVector<int> &ids);
    QString source() const { return m_source; }
    qint64 duration() const { return m_duration; }
    qint64 position() const { return m_position; }
    bool paused() const { return m_paused; }
    bool loaded() const { return m_loaded; }
    QString hardwareDecoder() const { return m_hardwareDecoder; }
    QVector<PlaybackTrack> audioTracks() const { return m_tracks; }
signals:
    void stateChanged();
    void tracksChanged();
    void frameRendered();
    void error(const QString &message);
    void doubleClicked();
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
private:
    friend class MpvSurface;
    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_render = nullptr;
    MpvSurface *m_surface = nullptr;
    QString m_source;
    QString m_hardwareDecoder;
    QImage m_frame;
    QVector<PlaybackTrack> m_tracks;
    QVector<int> m_selectedTracks;
    qint64 m_duration = 0, m_position = 0;
    qint64 m_rangeIn = 0, m_rangeOut = 0;
    qint64 m_seekTarget = -1;
    bool m_paused = true, m_loaded = false, m_software = false, m_loadPending = false;
    std::atomic<bool> m_updatePending{false}, m_eventsPending{false};
    bool m_tracksSelected = false, m_audioReloadPending = false, m_requestedPause = false;
    qint64 m_audioReloadPosition = 0;
    quint64 m_audioGeneration = 0;
    bool initialize();
    void initializeRender();
    void freeRender();
    void loadPending();
    void events();
    void renderSoftware();
    void command(const QStringList &args);
    void property(const char *name, const QString &value);
};
}
