#pragma once

#include <QJsonValue>
#include <QMap>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>

namespace Shadowless {

inline QString managedGsrProgram() {
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../engine/gpu-screen-recorder");
}

class CaptureAudio;

struct AudioLevels {
    int systemVolume = 100, microphoneVolume = 100, microphoneBoost = 0;
    int gameVolume = 100, musicVolume = 100;
    bool systemMuted = false, microphoneMuted = false;
    bool gameMuted = false, musicMuted = false;
};

struct GsrCapabilities {
    QString version, vendor, displayServer;
    QStringList codecs, sources;
    static GsrCapabilities parse(const QString &output);
    static GsrCapabilities probe(QString *error, const QString &program = managedGsrProgram());
};

struct GsrOptions {
    QString source = "portal", codec = "h264", container = "mp4", outputDirectory;
    QString overlayPlugin;
    QString audioOutput, audioInput, audioCodec = "aac";
    QMap<QString, int> applicationTracks;
    QMap<QString, bool> applicationIncluded, applicationGame;
    AudioLevels levels;
    int frameRate = 60, bufferSeconds = 30, bitrateMbps = 0;
    int audioBitrateKbps = 0, microphoneTrack = 1, pcTrack = 0;
    bool systemAudio = true, microphone = false, separateAudioTracks = false;
    bool applicationAudio = false, fourTrackAudio = false, continuousRecording = false;
    bool organizeByGame = false;
    bool dynamicApplicationAudio = true;
    bool dynamicAudioEnabled() const { return dynamicApplicationAudio && systemAudio && !applicationAudio && !fourTrackAudio; }
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
    void keepAlive(int timeoutMs = 30000);
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
                        const QString &program = managedGsrProgram());
    ~GsrCapture() override;
    bool running() const { return m_ready; }
    bool active() const { return m_preparingAudio || m_process.state() != QProcess::NotRunning; }
    bool busy() const { return m_request != nullptr || m_pausePending; }
    bool replayRecording() const { return m_replayRecording; }
    bool recordingPaused() const { return m_recordingPaused; }
    bool recordingPausePending() const { return m_pausePending; }
    bool recordingPauseKnown() const { return m_pauseKnown; }
    QString overlaySocketPath() const { return m_runtime.filePath("overlay.sock"); }
    QString applicationAudioStatePath() const { return m_runtime.filePath("audio-apps.json"); }
    QStringList applicationAudioSources() const { return m_applicationGames.keys(); }
    bool setApplicationAudioState(const QString &name, bool game, bool included);
    void start(const GsrOptions &options, const GsrCapabilities &capabilities);
    void saveReplay(int seconds);
    void startReplayRecording();
    void stopReplayRecording();
    void setRecordingPaused(bool paused);
    void setAudioLevels(const AudioLevels &levels);
    void stop();
signals:
    void started();
    void stopped();
    void clipSaved(const QString &path, bool recording);
    void error(const QString &message);
    void diagnostic(const QString &message);
    void recordingChanged(bool recording);
    void recordingPauseChanged();
    void authenticationChanged(bool pending);
    void applicationAudioDiscovered(const QString &name, bool game);
private:
    QString m_program, m_socketPath;
    GsrOptions m_options;
    QMap<QString, bool> m_applicationGames;
    QProcess m_process;
    std::unique_ptr<CaptureAudio> m_audio;
    QTemporaryDir m_runtime;
    QTimer m_readyTimer, m_startTimeout, m_stopTimeout;
    QTimer m_pauseTimeout;
    GsrRequest *m_request = nullptr;
    int m_nextId = 0;
    bool m_ready = false, m_stopping = false, m_stopQueued = false;
    bool m_replayRecording = false, m_preparingAudio = false;
    bool m_recordingPaused = false, m_pausePending = false, m_pauseKnown = true;
    QStringList m_logTail;
    QByteArray m_logBuffer;
    bool m_authenticating = false;
    void setAuthenticating(bool pending);
    void request(const QString &name, const QJsonValue &data = {});
    void startRecorder(const QString &output, const QString &input);
    void publishClip(const QString &path, bool recording);
    void readLog();
    void resetRecordingPause();
    bool writeApplicationAudioState();
};

} // namespace Shadowless
