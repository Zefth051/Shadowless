#pragma once

#include <QJsonValue>
#include <QMap>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

namespace Shadowless {

class CaptureAudio;

struct AudioLevels {
    int systemVolume = 100, microphoneVolume = 100, microphoneBoost = 0;
    bool systemMuted = false, microphoneMuted = false;
};

struct GsrCapabilities {
    QString version, vendor, displayServer;
    QStringList codecs, sources;
    static GsrCapabilities parse(const QString &output);
    static GsrCapabilities probe(QString *error, const QString &program = "gpu-screen-recorder");
};

struct GsrOptions {
    QString source = "portal", codec = "h264", container = "mp4", outputDirectory;
    QString overlayPlugin;
    QString audioOutput, audioInput, audioCodec = "aac";
    QMap<QString, int> applicationTracks;
    AudioLevels levels;
    int frameRate = 60, bufferSeconds = 30, bitrateMbps = 0;
    int audioBitrateKbps = 0, microphoneTrack = 1;
    bool systemAudio = true, microphone = false, separateAudioTracks = false;
    bool applicationAudio = false, continuousRecording = false;
    QString validate(const GsrCapabilities &capabilities) const;
    QStringList audioTracks() const;
    QStringList arguments(const QString &socketPath) const;
};

class GsrRequest : public QObject {
    Q_OBJECT
public:
    explicit GsrRequest(QObject *parent = nullptr);
    void send(const QString &socket, int id, const QString &name,
              const QJsonValue &data = {}, int timeoutMs = 30000);
signals:
    void finished(bool success, const QString &data);
private:
    QLocalSocket m_socket;
    QTimer m_timeout;
    QByteArray m_buffer;
    int m_id = 0;
    bool m_finished = false;
    void readReply();
    void finish(bool success, const QString &data);
};

class GsrCapture : public QObject {
    Q_OBJECT
public:
    explicit GsrCapture(QObject *parent = nullptr,
                        const QString &program = "gpu-screen-recorder");
    ~GsrCapture() override;
    bool running() const { return m_ready; }
    bool active() const { return m_preparingAudio || m_process.state() != QProcess::NotRunning; }
    bool busy() const { return m_request != nullptr; }
    bool replayRecording() const { return m_replayRecording; }
    QString overlaySocketPath() const { return m_runtime.filePath("overlay.sock"); }
    void start(const GsrOptions &options, const GsrCapabilities &capabilities);
    void saveReplay(int seconds);
    void startReplayRecording();
    void stopReplayRecording();
    void setAudioLevels(const AudioLevels &levels);
    void stop();
signals:
    void started();
    void stopped();
    void clipSaved(const QString &path);
    void error(const QString &message);
    void diagnostic(const QString &message);
    void recordingChanged(bool recording);
private:
    QString m_program, m_socketPath;
    GsrOptions m_options;
    QProcess m_process;
    std::unique_ptr<CaptureAudio> m_audio;
    QTemporaryDir m_runtime;
    QTimer m_readyTimer, m_startTimeout, m_stopTimeout;
    GsrRequest *m_request = nullptr;
    int m_nextId = 0;
    bool m_ready = false, m_stopping = false, m_stopQueued = false;
    bool m_replayRecording = false, m_preparingAudio = false;
    QStringList m_logTail;
    void request(const QString &name, const QJsonValue &data = {});
    void startRecorder(const QString &output, const QString &input);
    void publishClip(const QString &path);
    void readLog();
};

} // namespace Shadowless
