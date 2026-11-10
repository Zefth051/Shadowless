#include "gsr_capture.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>
#include <csignal>
#include <unistd.h>

namespace Captail {

GsrCapabilities GsrCapabilities::parse(const QString &output) {
    GsrCapabilities result;
    QString section;
    for (const QString &line : output.split('\n')) {
        const auto text = line.trimmed();
        if (text.startsWith("section=")) { section = text.mid(8); continue; }
        const auto fields = text.split('|');
        if (section == "system_info" && fields.size() >= 2) {
            if (fields[0] == "gsr_version") result.version = fields[1];
            if (fields[0] == "display_server") result.displayServer = fields[1];
        } else if (section == "gpu_info" && fields[0] == "vendor") {
            result.vendor = fields.value(1);
        } else if (section == "video_codecs" && QStringList{"h264", "hevc", "av1"}.contains(text)) {
            result.codecs.append(text);
        } else if (section == "capture_options") {
            static const QRegularExpression monitor("^[A-Za-z][A-Za-z0-9_-]*\\|[0-9]+x[0-9]+$");
            if (text == "portal" || text == "screen" || monitor.match(text).hasMatch())
                result.sources.append(fields[0]);
        }
    }
    result.codecs.removeDuplicates();
    result.sources.removeDuplicates();
    return result;
}

GsrCapabilities GsrCapabilities::probe(QString *error, const QString &program) {
    QProcess process;
    process.start(program, {"--info"});
    if (!process.waitForStarted(3000) || !process.waitForFinished(10000)) {
        *error = "Cannot query GPU Screen Recorder: " + process.errorString();
        process.kill(); process.waitForFinished(1000);
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = "GPU Screen Recorder capability query failed: " + QString::fromUtf8(process.readAllStandardError()).right(2000);
        return {};
    }
    auto result = parse(QString::fromUtf8(process.readAllStandardOutput()));
    if (result.codecs.isEmpty() || result.sources.isEmpty())
        *error = "GPU Screen Recorder reported no supported hardware codecs or screen sources.";
    else error->clear();
    return result;
}

QString GsrOptions::validate(const GsrCapabilities &capabilities) const {
    if (!capabilities.sources.contains(source)) return "This screen source is not available.";
    if (!capabilities.codecs.contains(codec)) return "This hardware codec is not available.";
    if (frameRate < 1 || frameRate > 240) return "Frame rate must be between 1 and 240.";
    if (bufferSeconds < 1 || bufferSeconds > 3600) return "Replay length must be between 1 and 3600 seconds.";
    if (bitrateMbps < 0 || bitrateMbps > 100) return "Bitrate must be automatic or between 1 and 100 Mbps.";
    if (container != "mp4" && container != "mkv") return "Choose MP4 or MKV.";
    if (outputDirectory.trimmed().isEmpty()) return "Choose an output directory.";
    return {};
}

QStringList GsrOptions::arguments(const QString &socketPath) const {
    QStringList args{"-w", source, "-f", QString::number(frameRate), "-k", codec,
        "-c", container, "-ac", "aac", "-fallback-cpu-encoding", "no", "-ipc", socketPath};
    QStringList audio;
    if (systemAudio) audio << "default_output";
    if (microphone) audio << "default_input";
    if (separateAudioTracks) { for (const auto &device : audio) args << "-a" << device; }
    else if (!audio.isEmpty()) args << "-a" << audio.join('|');
    if (bitrateMbps > 0) args << "-bm" << "cbr" << "-q" << QString::number(bitrateMbps * 1000);
    const QString output = QDir(outputDirectory).absolutePath();
    if (continuousRecording) {
        const auto name = "Recording_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss-zzz")
            + "_" + QUuid::createUuid().toString(QUuid::Id128).left(6) + "." + container;
        args << "-o" << QDir(output).filePath(name);
    } else {
        args << "-r" << QString::number(bufferSeconds) << "-restart-replay-on-save" << "no"
             << "-o" << output << "-ro" << output;
    }
    return args;
}

GsrRequest::GsrRequest(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    connect(&m_socket, &QLocalSocket::readyRead, this, &GsrRequest::readReply);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] {
        readReply();
        if (!m_finished) finish(false, "GPU Screen Recorder IPC: " + m_socket.errorString());
    });
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish(false, "GPU Screen Recorder command timed out; save completion is unknown."); });
}

void GsrRequest::send(const QString &socket, int id, const QString &name, const QJsonValue &data, int timeoutMs) {
    m_id = id;
    QJsonObject message{{"id", id}, {"name", name}};
    if (!data.isNull() && !data.isUndefined()) message["data"] = data;
    connect(&m_socket, &QLocalSocket::connected, this, [this, message] {
        m_socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    });
    m_timeout.start(timeoutMs);
    m_socket.connectToServer(socket);
}

void GsrRequest::readReply() {
    if (m_finished) return;
    m_buffer += m_socket.readAll();
    if (m_buffer.size() > 65536) { finish(false, "GPU Screen Recorder IPC reply is too large."); return; }
    const auto newline = m_buffer.indexOf('\n');
    if (newline < 0) return;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(m_buffer.left(newline), &parseError);
    const auto object = document.object();
    const auto result = object["result"].toString();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || !object["id"].isDouble() || object["id"].toDouble() != m_id
        || (result != "ok" && result != "error")
        || (!object["data"].isUndefined() && !object["data"].isNull() && !object["data"].isString())) {
        finish(false, "Invalid GPU Screen Recorder IPC reply.");
        return;
    }
    finish(result == "ok", object["data"].toString());
}

void GsrRequest::finish(bool success, const QString &data) {
    if (m_finished) return;
    m_finished = true;
    m_timeout.stop();
    m_socket.abort();
    emit finished(success, data);
}

GsrCapture::GsrCapture(QObject *parent, const QString &program) : QObject(parent), m_program(program) {
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_readyTimer.setInterval(100);
    m_startTimeout.setSingleShot(true);
    m_stopTimeout.setSingleShot(true);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &GsrCapture::readLog);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError failure) {
        if (failure == QProcess::FailedToStart) {
            m_readyTimer.stop(); m_startTimeout.stop();
            emit error("Cannot start GPU Screen Recorder: " + m_process.errorString());
            emit stopped();
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        readLog();
        m_readyTimer.stop(); m_startTimeout.stop(); m_stopTimeout.stop();
        const bool expected = m_stopping && code == 0 && status == QProcess::NormalExit;
        m_ready = false; m_stopping = false; m_stopQueued = false; m_replayRecording = false;
        if (!expected) emit error(QString("GPU Screen Recorder stopped (exit %1).\n%2").arg(code).arg(m_logTail.join('\n')));
        emit recordingChanged(false);
        emit stopped();
    });
    connect(&m_readyTimer, &QTimer::timeout, this, [this] {
        if (!QFileInfo::exists(m_socketPath)) return;
        QLocalSocket probe;
        probe.connectToServer(m_socketPath);
        if (!probe.waitForConnected(25)) return;
        m_readyTimer.stop(); m_startTimeout.stop();
        m_ready = true;
        emit started();
    });
    connect(&m_startTimeout, &QTimer::timeout, this, [this] {
        emit error("GPU Screen Recorder did not become ready within 60 seconds. Check capture permissions.");
        stop();
    });
    connect(&m_stopTimeout, &QTimer::timeout, this, [this] {
        emit error("GPU Screen Recorder did not stop cleanly; the current recording may be incomplete.");
        m_process.kill();
    });
    connect(&m_process, &QProcess::started, this, [this] {
        if (m_stopping) ::kill(static_cast<pid_t>(m_process.processId()), SIGINT);
    });
}

GsrCapture::~GsrCapture() {
    // Normal window close uses asynchronous stop(); this is only the final cleanup path.
    disconnect(&m_process, nullptr, this, nullptr);
    if (active()) {
        ::kill(static_cast<pid_t>(m_process.processId()), SIGINT);
        if (!m_process.waitForFinished(5000)) { m_process.kill(); m_process.waitForFinished(1000); }
    }
}

void GsrCapture::start(const GsrOptions &options, const GsrCapabilities &capabilities) {
    if (active() || m_request) { emit error("Capture is already active or finishing a command."); return; }
    const auto validation = options.validate(capabilities);
    if (!validation.isEmpty()) { emit error(validation); return; }
    if (!m_runtime.isValid() || !QDir().mkpath(options.outputDirectory)) {
        emit error("Cannot create the capture runtime or output directory."); return;
    }
    m_options = options;
    m_socketPath = m_runtime.filePath("gsr.sock");
    QFile::remove(m_socketPath);
    m_ready = false; m_stopping = false; m_stopQueued = false; m_replayRecording = false;
    m_logTail.clear();
    m_process.start(m_program, options.arguments(m_socketPath));
    m_readyTimer.start(); m_startTimeout.start(60000);
}

void GsrCapture::saveReplay(int seconds) {
    if (m_options.continuousRecording || seconds < 1 || seconds > m_options.bufferSeconds) {
        emit error("Choose a save duration within the active replay buffer."); return;
    }
    request("save-replay", QJsonObject{{"seconds", seconds}});
}

void GsrCapture::startReplayRecording() {
    if (m_options.continuousRecording) { emit error("Continuous recording is already selected."); return; }
    request("start-replay-recording");
}

void GsrCapture::stopReplayRecording() { request("stop-replay-recording"); }

void GsrCapture::stop() {
    if (!active() || m_stopping) return;
    if (m_request) { m_stopQueued = true; return; }
    m_readyTimer.stop(); m_startTimeout.stop();
    m_stopping = true;
    m_stopTimeout.start(15000);
    if (m_ready) request("stop");
    else if (m_process.processId() > 0) ::kill(static_cast<pid_t>(m_process.processId()), SIGINT);
}

void GsrCapture::request(const QString &name, const QJsonValue &data) {
    if (!m_ready) { emit error("Capture is not ready."); return; }
    if (m_request) { emit error("Another capture command is still pending."); return; }
    if (m_stopping && name != "stop") { emit error("Capture is stopping."); return; }
    auto *command = new GsrRequest(this);
    m_request = command;
    connect(command, &GsrRequest::finished, this, [this, command, name](bool success, const QString &result) {
        m_request = nullptr;
        command->deleteLater();
        if (!success) {
            emit error(result);
            if (name == "stop" && active()) ::kill(static_cast<pid_t>(m_process.processId()), SIGINT);
        } else {
            if (name == "save-replay" || name == "stop-replay-recording" || (name == "stop" && m_options.continuousRecording))
                publishClip(result);
            if (name == "start-replay-recording" || name == "stop-replay-recording") {
                m_replayRecording = name == "start-replay-recording";
                emit recordingChanged(m_replayRecording);
            }
        }
        if (m_stopQueued) { m_stopQueued = false; QTimer::singleShot(0, this, &GsrCapture::stop); }
    });
    command->send(m_socketPath, ++m_nextId, name, data);
}

void GsrCapture::publishClip(const QString &path) {
    const QFileInfo file(path);
    const auto relative = QDir(m_options.outputDirectory).relativeFilePath(file.absoluteFilePath());
    if (path.isEmpty() || !file.isFile() || file.size() == 0 || relative == ".." || relative.startsWith("../")) {
        emit error("GPU Screen Recorder did not confirm a non-empty clip in the output directory."); return;
    }
    emit clipSaved(file.absoluteFilePath());
}

void GsrCapture::readLog() {
    const auto output = QString::fromUtf8(m_process.readAllStandardOutput());
    for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
        m_logTail.append(line.left(1000));
        while (m_logTail.size() > 12) m_logTail.removeFirst();
        emit diagnostic(line);
    }
}

} // namespace Captail
