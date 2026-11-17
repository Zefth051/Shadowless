#include "gsr_capture.h"
#include "audio.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QUuid>
#include <csignal>
#include <unistd.h>

namespace Shadowless {

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
    static const QRegularExpression deviceId("^[A-Za-z0-9_][A-Za-z0-9_.:-]*$");
    if ((!dynamicAudioEnabled() && !audioOutput.isEmpty() && (!audioOutput.endsWith(".monitor") || !deviceId.match(audioOutput).hasMatch()))
        || (!audioInput.isEmpty() && (audioInput.endsWith(".monitor") || !deviceId.match(audioInput).hasMatch())))
        return "Choose a valid audio device.";
    if (levels.systemVolume < 0 || levels.systemVolume > 100 || levels.microphoneVolume < 0
        || levels.microphoneVolume > 100 || levels.microphoneBoost < 0 || levels.microphoneBoost > 20
        || levels.gameVolume < 0 || levels.gameVolume > 100 || levels.musicVolume < 0 || levels.musicVolume > 100)
        return "Audio levels are out of range.";
    if (audioCodec != "aac" && audioCodec != "opus") return "Choose AAC or Opus audio.";
    if (audioBitrateKbps != 0 && (audioBitrateKbps < 32 || audioBitrateKbps > 320))
        return "Audio bitrate must be automatic or between 32 and 320 kbps.";
    if (microphoneTrack < 1 || microphoneTrack > 6) return "Choose a valid microphone track.";
    if (pcTrack < 0 || pcTrack > 6) return "Choose a valid PC audio track.";
    if (fourTrackAudio) {
        if (!applicationAudio || pcTrack != 1 || !microphone || microphoneTrack != 4)
            return "Four-track audio requires PC on track 1 and the microphone enabled on track 4.";
        if (!applicationTracks.values().contains(2) || !applicationTracks.values().contains(3))
            return "Assign at least one game application to Game and one music application to Music.";
        QStringList selected;
        for (auto it = applicationTracks.cbegin(); it != applicationTracks.cend(); ++it) {
            if (it.value() != 2 && it.value() != 3)
                return "Four-track audio applications must use Game (track 2) or Music (track 3).";
            const auto name = it.key().trimmed().toCaseFolded();
            if (selected.contains(name)) return "Assign each application to only one audio role.";
            selected.append(name);
        }
    }
    if (applicationAudio && applicationTracks.isEmpty() && !pcTrack) return "Choose at least one application to record.";
    if (applicationAudio && pcTrack && (applicationTracks.values().contains(pcTrack) || (microphone && microphoneTrack == pcTrack)))
        return "PC audio needs its own track, separate from selected applications and the microphone.";
    for (auto it = applicationTracks.cbegin(); it != applicationTracks.cend(); ++it) {
        if (it.key().trimmed().isEmpty() || it.key().contains('|') || it.key().contains('\n')
            || it.key().contains('\r') || it.value() < 1 || it.value() > 6)
            return "Choose a valid application and track.";
    }
    return {};
}

QStringList GsrOptions::audioTracks() const {
    const auto source = [](const QString &device, const QString &fallback) {
        return device.isEmpty() ? fallback : "device:" + device;
    };
    const auto mic = microphone ? source(audioInput, "default_input") : QString();
    QStringList tracks;
    if (dynamicAudioEnabled()) {
        tracks.append("name:PC|app-inverse:");
        if (!mic.isEmpty()) tracks.append("name:Microphone|" + mic);
    } else if (applicationAudio) {
        QMap<int, QStringList> grouped;
        for (auto it = applicationTracks.cbegin(); it != applicationTracks.cend(); ++it)
            grouped[it.value()].append("app:" + it.key());
        if (!mic.isEmpty()) grouped[microphoneTrack].append(mic);
        if (pcTrack) {
            QStringList excluded = applicationTracks.keys();
            if (excluded.isEmpty()) excluded.append(QString());
            for (const auto &name : excluded) grouped[pcTrack].append("app-inverse:" + name);
        }
        if (fourTrackAudio) {
            const QStringList names{"PC", "Game", "Music", "Microphone"};
            for (int track = 1; track <= 4; ++track)
                tracks.append("name:" + names[track - 1] + '|' + grouped.value(track).join('|'));
        } else {
            for (const auto &group : grouped) tracks.append(group.join('|'));
        }
    } else {
        if (systemAudio) tracks.append(source(audioOutput, "default_output"));
        if (!mic.isEmpty()) tracks.append(mic);
        if (!separateAudioTracks && tracks.size() > 1) {
            tracks = {"name:PC + Microphone|" + tracks.join('|')};
        } else {
            for (int i = 0; i < tracks.size(); ++i)
                tracks[i].prepend((systemAudio && i == 0) ? "name:PC|" : "name:Microphone|");
        }
    }
    return tracks;
}

QStringList GsrOptions::arguments(const QString &socketPath) const {
    QStringList args{"-w", source, "-f", QString::number(frameRate), "-k", codec,
        "-c", container, "-ac", audioCodec, "-ab", QString::number(audioBitrateKbps),
        "-fallback-cpu-encoding", "no", "-ipc", socketPath};
    if (!overlayPlugin.isEmpty()) {
        // shortcut: GSR 6.1.5 cannot compose external cursor textures; restore cursor capture when its plugin path supports them.
        args << "-p" << overlayPlugin << "-cursor" << "no";
    }
    for (const auto &track : audioTracks()) args << "-a" << track;
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

GsrRequest::GsrRequest(QObject *parent) : QObject(parent), m_timeout(this) {
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

void GsrRequest::keepAlive(int timeoutMs) {
    if (!m_finished && m_timeout.isActive()) m_timeout.start(qMax(1, timeoutMs));
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

GsrCapture::GsrCapture(QObject *parent, const QString &program)
    : QObject(parent), m_program(program), m_audio(std::make_unique<CaptureAudio>()) {
    connect(m_audio.get(), &CaptureAudio::ready, this, &GsrCapture::startRecorder);
    connect(m_audio.get(), &CaptureAudio::failed, this, [this](const QString &message) {
        emit error(message);
        stop();
    });
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_readyTimer.setInterval(100);
    m_startTimeout.setSingleShot(true);
    m_stopTimeout.setSingleShot(true);
    m_pauseTimeout.setSingleShot(true);
    connect(&m_pauseTimeout, &QTimer::timeout, this, [this] {
        m_pausePending = false; m_pauseKnown = false;
        emit recordingPauseChanged();
        emit error("Recording pause state could not be confirmed. Stop and save the recording before starting again.");
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &GsrCapture::readLog);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError failure) {
        if (failure == QProcess::FailedToStart) {
            m_readyTimer.stop(); m_startTimeout.stop();
            m_audio->stop();
            emit error("Cannot start GPU Screen Recorder: " + m_process.errorString());
            setAuthenticating(false);
            emit stopped();
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        readLog();
        setAuthenticating(false);
        for (const auto &name : QDir(m_runtime.path()).entryList({"shadowless-kms-*"}, QDir::Dirs | QDir::NoDotAndDotDot))
            QDir(m_runtime.filePath(name)).removeRecursively();
        m_readyTimer.stop(); m_startTimeout.stop(); m_stopTimeout.stop();
        m_audio->stop();
        const bool expected = m_stopping && code == 0 && status == QProcess::NormalExit;
        m_ready = false; m_stopping = false; m_stopQueued = false; m_replayRecording = false;
        resetRecordingPause();
        if (!expected) emit error(QString("GPU Screen Recorder stopped (exit %1).\n%2").arg(code).arg(m_logTail.join('\n')));
        emit recordingChanged(false);
        emit stopped();
    });
    connect(&m_readyTimer, &QTimer::timeout, this, [this] {
        if (!QFileInfo::exists(m_socketPath)) return;
        if (!m_options.overlayPlugin.isEmpty() && !QFileInfo::exists(overlaySocketPath())) return;
        QLocalSocket probe;
        probe.connectToServer(m_socketPath);
        if (!probe.waitForConnected(25)) return;
        m_readyTimer.stop(); m_startTimeout.stop();
        m_ready = true;
        setAuthenticating(false);
        emit started();
    });
    connect(&m_startTimeout, &QTimer::timeout, this, [this] {
        const bool missingCompositor = !m_options.overlayPlugin.isEmpty() && !QFileInfo::exists(overlaySocketPath());
        emit error(missingCompositor
            ? "The Shadowless composition plugin did not become ready.\n" + m_logTail.join('\n')
            : "GPU Screen Recorder did not become ready within 60 seconds. Check capture permissions.");
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
    if (m_process.state() != QProcess::NotRunning) {
        ::kill(static_cast<pid_t>(m_process.processId()), SIGINT);
        if (!m_process.waitForFinished(5000)) { m_process.kill(); m_process.waitForFinished(1000); }
    }
}

void GsrCapture::start(const GsrOptions &options, const GsrCapabilities &capabilities) {
    if (active() || m_request) { emit error("Capture is already active or finishing a command."); return; }
    const auto validation = options.validate(capabilities);
    if (!validation.isEmpty()) { emit error(validation); return; }
    if (!options.overlayPlugin.isEmpty() && !QFileInfo(options.overlayPlugin).isFile()) {
        emit error("Shadowless's overlay plugin is missing: " + options.overlayPlugin); return;
    }
    if (!m_runtime.isValid() || !QDir().mkpath(options.outputDirectory)) {
        emit error("Cannot create the capture runtime or output directory."); return;
    }
    m_options = options;
    m_applicationGames.clear();
    if (options.dynamicAudioEnabled() && !writeApplicationAudioState()) {
        emit error("Could not prepare application audio selection."); return;
    }
    m_preparingAudio = true;
    m_audio->start(options);
}

void GsrCapture::startRecorder(const QString &output, const QString &input) {
    if (!m_preparingAudio) return;
    m_preparingAudio = false;
    auto options = m_options;
    if (!output.isEmpty()) options.audioOutput = output;
    if (!input.isEmpty()) options.audioInput = input;
    m_socketPath = m_runtime.filePath("gsr.sock");
    QFile::remove(m_socketPath);
    QFile::remove(overlaySocketPath());
    QFile::remove(overlaySocketPath() + ".preview");
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("SHADOWLESS_AUDIO_LEVELS_FILE", m_audio->levelsPath());
    environment.insert("SHADOWLESS_FOUR_TRACK_AUDIO", options.fourTrackAudio || options.dynamicAudioEnabled() ? "1" : "0");
    environment.insert("SHADOWLESS_DYNAMIC_AUDIO", options.dynamicAudioEnabled() ? "1" : "0");
    environment.insert("SHADOWLESS_AUDIO_APPS_FILE", applicationAudioStatePath());
    environment.insert("SHADOWLESS_MICROPHONE_SOURCE", options.microphone
        ? (options.audioInput.isEmpty() ? "default_input" : options.audioInput) : QString());
    environment.insert("SHADOWLESS_OVERLAY_SOCKET", overlaySocketPath());
    environment.insert("SHADOWLESS_GSR_KMS_RUNTIME", m_runtime.path());
    m_process.setProcessEnvironment(environment);
    m_ready = false; m_stopping = false; m_stopQueued = false; m_replayRecording = false;
    resetRecordingPause();
    m_logTail.clear(); m_logBuffer.clear();
    setAuthenticating(false);
    m_readyTimer.start(); m_startTimeout.start(60000);
    m_process.start(m_program, options.arguments(m_socketPath));
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

void GsrCapture::setRecordingPaused(bool paused) {
    if (!m_options.continuousRecording) { emit error("Pause is available only for recording without Instant Replay."); return; }
    if (!m_ready || m_stopping || busy() || m_pausePending) { emit error("Capture is not ready to change recording pause state."); return; }
    if (!m_pauseKnown) { emit error("Recording pause state is unknown. Stop and save the recording first."); return; }
    if (paused == m_recordingPaused) return;
    m_pausePending = true;
    m_pauseTimeout.start(3000);
    request("set-paused", paused);
    emit recordingPauseChanged();
}

void GsrCapture::resetRecordingPause() {
    m_pauseTimeout.stop();
    const bool changed = m_recordingPaused || m_pausePending || !m_pauseKnown;
    m_recordingPaused = false; m_pausePending = false; m_pauseKnown = true;
    if (changed) emit recordingPauseChanged();
}

void GsrCapture::setAudioLevels(const AudioLevels &levels) {
    if (active()) m_audio->setLevels(levels);
}

bool GsrCapture::writeApplicationAudioState() {
    QJsonObject selections;
    auto names = m_options.applicationIncluded.keys();
    names.append(m_options.applicationGame.keys()); names.append(m_applicationGames.keys());
    names.removeDuplicates();
    for (const auto &name : names) selections.insert(name, QJsonObject{
        {"game", m_options.applicationGame.value(name, m_applicationGames.value(name, false))},
        {"included", m_options.applicationIncluded.value(name, true)}});
    QSaveFile file(applicationAudioStatePath());
    const auto bytes = QJsonDocument(selections).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool GsrCapture::setApplicationAudioState(const QString &name, bool game, bool included) {
    if (!m_options.dynamicAudioEnabled() || name.isEmpty() || name.size() > 512
        || name.contains(QChar::Null) || name.contains('\n') || name.contains('\r')) return false;
    const auto previous = m_options;
    m_options.applicationGame.insert(name, game); m_options.applicationIncluded.insert(name, included);
    if (writeApplicationAudioState()) return true;
    m_options = previous;
    emit error("Could not update application audio selection."); return false;
}

void GsrCapture::stop() {
    if (!active() || m_stopping) return;
    if (m_preparingAudio) {
        m_preparingAudio = false;
        m_audio->stop();
        emit stopped();
        return;
    }
    if (m_request) { m_stopQueued = true; return; }
    if (m_replayRecording) {
        m_stopQueued = true;
        stopReplayRecording();
        return;
    }
    m_readyTimer.stop(); m_startTimeout.stop();
    m_stopping = true;
    m_stopTimeout.start(m_options.dynamicAudioEnabled() ? 30000 : 15000);
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
            if (name == "set-paused" && m_pausePending) {
                m_pauseTimeout.stop(); m_pausePending = false; m_pauseKnown = false;
            }
            if (name == "stop-replay-recording" && m_stopQueued) m_replayRecording = false;
            emit error(result);
            if (name == "stop" && active()) ::kill(static_cast<pid_t>(m_process.processId()), SIGINT);
        } else {
            if (name == "save-replay" || name == "stop-replay-recording" || (name == "stop" && m_options.continuousRecording))
                publishClip(result, name != "save-replay");
            if (name == "start-replay-recording" || name == "stop-replay-recording") {
                m_replayRecording = name == "start-replay-recording";
                emit recordingChanged(m_replayRecording);
            }
        }
        if (name == "set-paused") emit recordingPauseChanged();
        if (m_stopQueued) { m_stopQueued = false; QTimer::singleShot(0, this, &GsrCapture::stop); }
    });
    command->send(m_socketPath, ++m_nextId, name, data);
}

void GsrCapture::publishClip(const QString &path, bool recording) {
    const QFileInfo file(path);
    const auto relative = QDir(m_options.outputDirectory).relativeFilePath(file.absoluteFilePath());
    if (path.isEmpty() || !file.isFile() || file.size() == 0 || relative == ".." || relative.startsWith("../")) {
        emit error("GPU Screen Recorder did not confirm a non-empty clip in the output directory."); return;
    }
    emit clipSaved(file.absoluteFilePath(), recording);
}

void GsrCapture::setAuthenticating(bool pending) {
    if (m_authenticating == pending) return;
    m_authenticating = pending;
    emit authenticationChanged(pending);
}

void GsrCapture::readLog() {
    m_logBuffer += m_process.readAllStandardOutput();
    if (m_process.state() == QProcess::NotRunning && !m_logBuffer.isEmpty()) m_logBuffer += '\n';
    int newline;
    while ((newline = m_logBuffer.indexOf('\n')) >= 0) {
        const auto line = QString::fromUtf8(m_logBuffer.left(qMin(newline, 4096))).trimmed();
        m_logBuffer.remove(0, newline + 1);
        if (line.isEmpty()) continue;
        if (!m_options.continuousRecording && (line == "gsr info: Started recording"
            || line == "gsr info: Stopped recording" || line == "gsr error: Failed to save recording")) {
            const bool recording = line == "gsr info: Started recording";
            if (m_replayRecording != recording) {
                m_replayRecording = recording;
                emit recordingChanged(recording);
            }
        }
        if (m_options.dynamicAudioEnabled() && line == "gsr info: shadowless-audio-finalizing: progress") {
            if (m_request) m_request->keepAlive();
            if (m_stopping) m_stopTimeout.start(30000);
        }
        const QString appPrefix = "gsr info: shadowless-audio-app: ";
        if (m_options.dynamicAudioEnabled() && line.startsWith(appPrefix)) {
            const auto app = QJsonDocument::fromJson(line.mid(appPrefix.size()).toUtf8()).object();
            const auto name = app.value("name").toString();
            if (!name.isEmpty() && name.size() <= 512 && !name.contains(QChar::Null)
                && !name.contains('\n') && !name.contains('\r') && app.value("game").isBool()) {
                const bool game = m_options.applicationGame.value(name, app.value("game").toBool());
                if (!m_applicationGames.contains(name) || m_applicationGames.value(name) != game) {
                    m_applicationGames.insert(name, game);
                    emit applicationAudioDiscovered(name, game);
                }
            }
        }
        if (line == "gsr info: shadowless-kms-auth: begin") setAuthenticating(true);
        else if (line == "gsr info: shadowless-kms-auth: end") setAuthenticating(false);
        // IPC acknowledges queuing; the recorder log confirms that pause was applied.
        if (m_options.continuousRecording && (line == "gsr info: Paused" || line == "gsr info: Unpaused")) {
            m_recordingPaused = line == "gsr info: Paused";
            m_pausePending = false; m_pauseKnown = true; m_pauseTimeout.stop();
            emit recordingPauseChanged();
        }
        m_logTail.append(line.left(1000));
        while (m_logTail.size() > 12) m_logTail.removeFirst();
        emit diagnostic(line);
    }
    m_logBuffer.truncate(4096);
}

} // namespace Shadowless
