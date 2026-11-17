#include "audio.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
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
    const auto streams = run("pactl", {"-f", "json", "list", "sink-inputs"});
    if (streams.ok) {
        for (const auto &entry : QJsonDocument::fromJson(streams.output.toUtf8()).array()) {
            const auto properties = entry.toObject().value("properties").toObject();
            const auto name = properties.value("application.name").toString();
            if (!catalog.applications.contains(name)) continue;
            const auto icon = properties.value("application.icon_name").toString().trimmed();
            if (!icon.isEmpty()) catalog.applicationIcons.insert(name, icon);
            else if (!catalog.applicationIcons.contains(name)) {
                const auto binary = QFileInfo(properties.value("application.process.binary").toString()).fileName();
                if (!binary.isEmpty()) catalog.applicationIcons.insert(name, binary);
            }
        }
    }
    return catalog;
}

CaptureAudio::CaptureAudio(QObject *parent) : QObject(parent) {
    m_follow.setInterval(1500);
    connect(&m_follow, &QTimer::timeout, this, &CaptureAudio::followMicrophone);
}

CaptureAudio::~CaptureAudio() { stop(); }

bool CaptureAudio::writeLevels(const AudioLevels &levels) {
    QSaveFile file(levelsPath());
    if (!m_runtime.isValid() || !file.open(QIODevice::WriteOnly)) return false;
    const double system = levels.systemMuted ? 0.0 : levels.systemVolume / 100.0;
    const double microphone = levels.microphoneMuted ? 0.0
        : levels.microphoneVolume / 100.0 * std::pow(10.0, levels.microphoneBoost / 20.0);
    const double game = levels.gameMuted ? 0.0 : levels.gameVolume / 100.0;
    const double music = levels.musicMuted ? 0.0 : levels.musicVolume / 100.0;
    const auto data = (QString::number(system, 'g', 12) + ' '
        + QString::number(microphone, 'g', 12) + ' ' + QString::number(game, 'g', 12)
        + ' ' + QString::number(music, 'g', 12) + '\n').toLatin1();
    return file.write(data) == data.size() && file.commit();
}

// The recorder reads a fixed virtual source; a loopback feeds it from whichever microphone is usable right now,
// so unplugging or re-plugging the chosen microphone never needs a recorder restart.
QString CaptureAudio::desiredMicrophone() const {
    const auto sources = run("pactl", {"list", "short", "sources"});
    if (!sources.ok) return m_current;
    QStringList names;
    for (const auto &line : sources.output.split('\n', Qt::SkipEmptyParts)) {
        const auto name = line.section('\t', 1, 1);
        if (!name.isEmpty() && !name.endsWith(".monitor") && !name.startsWith("shadowless_")) names << name;
    }
    if (!m_selected.isEmpty() && names.contains(m_selected)) return m_selected;
    const auto fallback = run("pactl", {"get-default-source"}).output;
    return names.contains(fallback) ? fallback : (names.isEmpty() ? QString() : names.first());
}

void CaptureAudio::routeMicrophone(const QString &source) {
    if (!m_loopModule.isEmpty()) run("pactl", {"unload-module", m_loopModule});
    m_loopModule.clear(); m_current = source;
    if (source.isEmpty()) return; // no microphone connected at all: the track stays silent until one appears
    const auto module = run("pactl", {"load-module", "module-loopback", "source=" + source, "sink=shadowless_mic", "latency_msec=20"});
    if (module.ok) m_loopModule = module.output;
    else m_current.clear(); // retry on the next poll
}

void CaptureAudio::followMicrophone() {
    const auto wanted = desiredMicrophone();
    if (wanted != m_current) routeMicrophone(wanted);
}

void CaptureAudio::releaseMicrophone() {
    m_follow.stop();
    if (!m_loopModule.isEmpty()) run("pactl", {"unload-module", m_loopModule});
    if (!m_sinkModule.isEmpty()) run("pactl", {"unload-module", m_sinkModule});
    m_loopModule.clear(); m_sinkModule.clear(); m_current.clear();
}

void CaptureAudio::start(const GsrOptions &options) {
    if (!writeLevels(options.levels)) {
        emit failed("Could not prepare the private recording audio levels.");
        return;
    }
    releaseMicrophone();
    if (options.microphone) {
        m_selected = options.audioInput;
        const auto sink = run("pactl", {"load-module", "module-null-sink", "sink_name=shadowless_mic",
                                        "sink_properties=device.description=Shadowless_microphone", "media.class=Audio/Sink"});
        if (sink.ok) {
            m_sinkModule = sink.output;
            routeMicrophone(desiredMicrophone());
            m_follow.start();
            emit ready({}, "shadowless_mic.monitor");
            return;
        }
        // Without pactl the selected device is used directly, as before.
    }
    emit ready({}, {});
}

void CaptureAudio::setLevels(const AudioLevels &levels) {
    if (!writeLevels(levels)) emit failed("Could not update recording audio levels. Capture is stopping.");
}

void CaptureAudio::stop() { QFile::remove(levelsPath()); releaseMicrophone(); }
} // namespace Shadowless
