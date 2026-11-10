#pragma once

#include <QJsonValue>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

namespace Captail {

struct GsrCapabilities {
    QString version, vendor, displayServer;
    QStringList codecs, sources;
    static GsrCapabilities parse(const QString &output);
    static GsrCapabilities probe(QString *error, const QString &program = "gpu-screen-recorder");
};

struct GsrOptions {
    QString source = "portal", codec = "h264", container = "mp4", outputDirectory;
    int frameRate = 60, bufferSeconds = 30, bitrateMbps = 0;
    bool systemAudio = true, microphone = false, separateAudioTracks = false;
    bool continuousRecording = false;
    QString validate(const GsrCapabilities &capabilities) const;
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
    bool active() const { return m_process.state() != QProcess::NotRunning; }
    bool busy() const { return m_request != nullptr; }
    void start(const GsrOptions &options, const GsrCapabilities &capabilities);
    void saveReplay(int seconds);
    void startReplayRecording();
    void stopReplayRecording();
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
    QTemporaryDir m_runtime;
    QTimer m_readyTimer, m_startTimeout, m_stopTimeout;
    GsrRequest *m_request = nullptr;
    int m_nextId = 0;
    bool m_ready = false, m_stopping = false, m_stopQueued = false;
    bool m_replayRecording = false;
    QStringList m_logTail;
    void request(const QString &name, const QJsonValue &data = {});
    void publishClip(const QString &path);
    void readLog();
};

} // namespace Captail
