#include "audio.h"
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QtConcurrentRun>
#include <QThread>
#include <QUuid>
#include <cmath>

namespace Shadowless {
namespace {
struct CommandResult {
    QString output;
    bool ok = false;
};

CommandResult run(const QString &program, const QStringList &args) {
    QProcess command;
    command.start(program, args);
    if (!command.waitForStarted(1500)) return {};
    if (!command.waitForFinished(1500)) {
        command.kill();
        command.waitForFinished(500);
        return {};
    }
    return {QString::fromUtf8(command.readAllStandardOutput()).trimmed(),
            command.exitStatus() == QProcess::NormalExit && command.exitCode() == 0};
}

struct Targets {
    QString output, input, error;
};

Targets resolveTargets(const GsrOptions &options) {
    Targets targets;
    if (options.systemAudio && !options.applicationAudio) {
        targets.output = options.audioOutput.isEmpty() ? run("pactl", {"get-default-sink"}).output
                                                       : options.audioOutput.chopped(8);
        if (targets.output.isEmpty() || !run("pactl", {"get-sink-volume", targets.output}).ok)
            targets.error = "The selected audio output is unavailable.";
    }
    if (options.microphone) {
        targets.input = options.audioInput.isEmpty() ? run("pactl", {"get-default-source"}).output
                                                     : options.audioInput;
        if (targets.input.isEmpty() || !run("pactl", {"get-source-volume", targets.input}).ok)
            targets.error = "The selected microphone is unavailable.";
    }
    return targets;
}

bool applyGain(const QString &name, int volume, int boost, bool muted) {
    if (name.isEmpty()) return true;
    const double gain = muted ? 0.0 : volume / 100.0 * std::pow(10.0, boost / 20.0);
    // PulseAudio's software volume scale is cubic. The UI uses linear amplitude and dB.
    const auto pulse = QString::number(qRound(65536.0 * std::cbrt(gain)));
    return run("pactl", {"set-source-volume", name, pulse}).ok;
}
} // namespace

AudioCatalog AudioCatalog::probe(const QString &program) {
    AudioCatalog catalog;
    const auto devices = run(program, {"--list-audio-devices"});
    const auto apps = run(program, {"--list-application-audio"});
    if (!devices.ok) catalog.error = "Could not list audio devices.";
    if (!apps.ok) catalog.error += (catalog.error.isEmpty() ? "" : " ") + QString("Could not list applications playing audio.");
    for (const auto &line : devices.output.split('\n', Qt::SkipEmptyParts)) {
        const auto divider = line.indexOf('|');
        if (divider <= 0) continue;
        const auto id = line.left(divider);
        if (id == "default_output" || id == "default_input" || id.startsWith("shadowless_")) continue;
        auto &list = id.endsWith(".monitor") ? catalog.outputs : catalog.inputs;
        list.append({id, line.mid(divider + 1)});
    }
    for (const auto &name : apps.output.split('\n', Qt::SkipEmptyParts))
        if (!name.contains('|') && !name.contains('\r')) catalog.applications.append(name);
    catalog.applications.removeDuplicates();
    catalog.applications.sort(Qt::CaseInsensitive);
    return catalog;
}

CaptureAudio::CaptureAudio(QObject *parent) : QObject(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(60);
    connect(&m_debounce, &QTimer::timeout, this, &CaptureAudio::applyLevels);
}

CaptureAudio::~CaptureAudio() {
    stop();
    for (auto *process : findChildren<QProcess *>()) {
        if (process->state() != QProcess::NotRunning && !process->waitForFinished(500)) {
            process->kill();
            process->waitForFinished(500);
        }
    }
}

void CaptureAudio::start(const GsrOptions &options) {
    m_levels = options.levels;
    if ((!options.systemAudio || options.applicationAudio) && !options.microphone) {
        emit ready({}, {});
        return;
    }
    if (QStandardPaths::findExecutable("pactl").isEmpty() || QStandardPaths::findExecutable("pw-loopback").isEmpty()) {
        emit failed("Capture audio requires pactl and pw-loopback.");
        return;
    }
    const int generation = ++m_generation;
    auto *resolution = new QFutureWatcher<Targets>(this);
    connect(resolution, &QFutureWatcher<Targets>::finished, this, [this, resolution, generation] {
        const auto targets = resolution->result();
        resolution->deleteLater();
        if (generation != m_generation) return;
        if (!targets.error.isEmpty()) {
            emit failed(targets.error);
            return;
        }
        if (!targets.output.isEmpty()) launch(m_output, targets.output, true);
        if (generation != m_generation) return;
        if (!targets.input.isEmpty()) launch(m_input, targets.input, false);
        if (generation == m_generation) applyLevels();
    });
    resolution->setFuture(QtConcurrent::run(resolveTargets, options));
}

void CaptureAudio::launch(Route &route, const QString &target, bool output) {
    route.name = "shadowless_" + QUuid::createUuid().toString(QUuid::Id128);
    auto *process = new QProcess(this);
    route.process = process;
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            emit failed("Cannot start pw-loopback. Install the PipeWire command-line tools.");
    });
    connect(process, &QProcess::finished, this, [this] {
        emit failed("The PipeWire capture audio route stopped.");
    });
    const auto label = output ? "Shadowless desktop audio" : "Shadowless microphone";
    const QString privateState = "state.restore-props=false state.restore-target=false";
    QString capture = privateState + " node.passive=true node.dont-fallback=true node.dont-reconnect=true";
    if (output) capture += " stream.capture.sink=true";
    const auto playback = QString("%1 media.class=Audio/Source node.name=%2 node.description=\"%3\" priority.session=0 node.virtual=true")
        .arg(privateState, route.name, label);
    process->start("pw-loopback", {"-n", route.name, "-C", target, "-i", capture, "-o", playback});
}

void CaptureAudio::setLevels(const AudioLevels &levels) {
    m_levels = levels;
    m_dirty = true;
    if (m_ready && !m_updating) m_debounce.start();
}

void CaptureAudio::applyLevels() {
    const auto output = m_output.name, input = m_input.name;
    if (output.isEmpty() && input.isEmpty()) return;
    const auto levels = m_levels;
    const bool starting = !m_ready;
    const int generation = m_generation;
    m_dirty = false;
    m_updating = true;
    const auto cancelled = m_cancelUpdate = std::make_shared<std::atomic_bool>(false);
    auto *update = new QFutureWatcher<bool>(this);
    connect(update, &QFutureWatcher<bool>::finished, this, [this, update, generation, output, input] {
        const bool success = update->result();
        update->deleteLater();
        if (generation != m_generation) return;
        m_cancelUpdate.reset();
        m_updating = false;
        if (!success) {
            if (!m_ready) emit failed("Could not prepare capture audio. Check that PipeWire and pipewire-pulse are running.");
            else emit failed("Could not apply the capture audio levels. Capture is stopping.");
        } else if (!m_ready && !m_dirty) {
            m_ready = true;
            emit ready(output, input);
        }
        if (generation == m_generation && m_dirty) m_debounce.start();
    });
    update->setFuture(QtConcurrent::run([output, input, levels, starting, cancelled] {
        QElapsedTimer elapsed;
        elapsed.start();
        const int attempts = starting ? 25 : 3;
        for (int attempt = 0; attempt < attempts && elapsed.elapsed() < 2500 && !*cancelled; ++attempt) {
            const bool systemOk = applyGain(output, levels.systemVolume, 0, levels.systemMuted);
            if (*cancelled) return false;
            const bool micOk = applyGain(input, levels.microphoneVolume, levels.microphoneBoost, levels.microphoneMuted);
            if (systemOk && micOk) return true;
            if (attempt + 1 < attempts) QThread::msleep(80);
        }
        return false;
    }));
}

void CaptureAudio::stop() {
    ++m_generation;
    if (m_cancelUpdate) { *m_cancelUpdate = true; m_cancelUpdate.reset(); }
    m_debounce.stop();
    m_ready = false;
    m_updating = false;
    m_dirty = false;
    for (auto *route : {&m_output, &m_input}) {
        auto *process = route->process;
        route->name.clear();
        route->process = nullptr;
        if (!process) continue;
        disconnect(process, nullptr, this, nullptr);
        if (process->state() == QProcess::NotRunning) {
            process->deleteLater();
            continue;
        }
        connect(process, &QProcess::started, process, &QProcess::terminate);
        connect(process, &QProcess::finished, process, &QProcess::deleteLater);
        connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) process->deleteLater();
        });
        if (process->state() == QProcess::Running) process->terminate();
        QTimer::singleShot(1500, process, [process] {
            if (process->state() != QProcess::NotRunning) process->kill();
        });
    }
}
} // namespace Shadowless
