#pragma once
#include <QObject>
#include <QImage>
#include <QProcess>
#include <QSize>
#include <QVector>
#include <functional>

namespace Shadowless {
struct AudioStream { int index = -1; QString title, codec; int channels = 0; };
struct MediaInfo {
    QString path;
    qint64 durationMs = 0;
    QSize videoSize;
    QVector<AudioStream> audioStreams;
};
struct ExportRequest {
    QString sourcePath, destinationPath;
    qint64 inMs = 0, outMs = 0;
    // Audio ordinals, matching MediaInfo::audioStreams; empty means silent output.
    QVector<int> audioTracks;
    bool mixAudio = false, overwrite = false;
};
class MediaTools : public QObject {
    Q_OBJECT
public:
    explicit MediaTools(QObject *parent = nullptr);
    ~MediaTools() override;
    bool analyze(const QString &path);
    bool exportClip(const ExportRequest &request);
    bool busy() const { return m_busy; }
    void stop();
signals:
    void metadataReady(const Shadowless::MediaInfo &info);
    void thumbnailsReady(const QVector<QImage> &images);
    void waveformReady(int audioOrdinal, const QVector<float> &peaks);
    void progress(double fraction);
    void exported(const QString &path);
    void failed(const QString &message);
    void cancelled();
    void finished();
private:
    void launch(const QString &program, const QStringList &args,
                std::function<void()> done, bool waveform = false);
    void probe(const QString &path, std::function<void(MediaInfo)> done);
    void nextThumbnail();
    void nextWaveform();
    void consume();
    void fail(const QString &message);
    void finish();
    void cleanup();
    QProcess *m_process;
    QByteArray m_output, m_errors, m_pcm;
    std::function<void()> m_done;
    MediaInfo m_info;
    QVector<QImage> m_thumbnails;
    QVector<float> m_peaks;
    int m_thumbnail = 0, m_audio = 0;
    qint64 m_sample = 0;
    quint64 m_generation = 0;
    bool m_busy = false, m_stopping = false, m_waveform = false;
    QString m_tempPath;
};
}
Q_DECLARE_METATYPE(Shadowless::MediaInfo)
Q_DECLARE_METATYPE(QVector<QImage>)
Q_DECLARE_METATYPE(QVector<float>)
