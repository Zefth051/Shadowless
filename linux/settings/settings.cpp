#include "settings.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

namespace Shadowless {
Settings::Settings(const QString &path) {
    if (!path.isEmpty()) {
        m_store = std::make_unique<QSettings>(path, QSettings::IniFormat);
        return;
    }

    m_store = std::make_unique<QSettings>("Shadowless", "Shadowless");
    m_store->setFallbacksEnabled(false);
    if (QFileInfo::exists(m_store->fileName())) return;

    QSettings legacy("Captail", "LinuxGsrSpike");
    legacy.setFallbacksEnabled(false);
    for (const auto &key : legacy.allKeys())
        m_store->setValue(key, legacy.value(key));
    m_store->sync();
}
GsrOptions Settings::capture(const GsrCapabilities &caps) const {
    GsrOptions o; const auto &s = *m_store;
    o.source = s.value("source", caps.sources.value(0)).toString();
    o.codec = s.value("codec", "h264").toString();
    if (!caps.sources.contains(o.source)) o.source = caps.sources.value(0);
    if (!caps.codecs.contains(o.codec)) o.codec = caps.codecs.value(0);
    o.container = s.value("container", "mp4").toString();
    if (o.container != "mp4" && o.container != "mkv") o.container = "mp4";
    o.frameRate = qBound(1, s.value("fps", 60).toInt(), 240);
    o.bufferSeconds = qBound(1, s.value("bufferSeconds", 30).toInt(), 3600);
    o.bitrateMbps = qBound(0, s.value("bitrateMbps", 0).toInt(), 100);
    o.systemAudio = s.value("systemAudio", true).toBool(); o.microphone = s.value("microphone", false).toBool();
    o.audioOutput = s.value("audio/output").toString();
    o.audioInput = s.value("audio/input").toString();
    o.audioCodec = s.value("audio/codec", "aac").toString();
    if (o.audioCodec != "opus") o.audioCodec = "aac";
    const int audioBitrate = s.value("audio/bitrateKbps", 0).toInt();
    o.audioBitrateKbps = audioBitrate == 0 ? 0 : qBound(32, audioBitrate, 320);
    o.microphoneTrack = qBound(1, s.value("audio/microphoneTrack", 1).toInt(), 6);
    o.pcTrack = qBound(0, s.value("audio/pcTrack", 0).toInt(), 6);
    o.applicationAudio = s.value("audio/applicationMode", false).toBool();
    const auto routes = s.value("audio/applicationTracks").toMap();
    for (auto it = routes.cbegin(); it != routes.cend(); ++it) o.applicationTracks.insert(it.key(), it.value().toInt());
    o.levels.systemVolume = qBound(0, s.value("audio/systemVolume", 100).toInt(), 100);
    o.levels.microphoneVolume = qBound(0, s.value("audio/microphoneVolume", 100).toInt(), 100);
    o.levels.microphoneBoost = qBound(0, s.value("audio/microphoneBoost", 0).toInt(), 20);
    o.levels.systemMuted = s.value("audio/systemMuted", false).toBool();
    o.levels.microphoneMuted = s.value("audio/microphoneMuted", false).toBool();
    o.separateAudioTracks = s.value("separateAudioTracks", false).toBool();
    o.continuousRecording = s.value("continuousRecording", false).toBool();
    o.organizeByGame = s.value("organizeByGame", false).toBool();
    auto videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (videos.isEmpty()) videos = QDir::home().filePath("Videos");
    const auto preferred = QDir(videos).filePath("Shadowless");
    const auto legacyOutput = QDir(videos).filePath("Captail");
    const bool hasLegacy = QDir(legacyOutput).exists();
    const auto fallback = !QDir(preferred).exists() && hasLegacy ? legacyOutput : preferred;
    o.outputDirectory = s.value("outputDirectory", fallback).toString();
    if (o.outputDirectory == legacyOutput && !hasLegacy && QDir(preferred).exists())
        o.outputDirectory = preferred;
    return o;
}
OverlayScene Settings::scene() const {
    OverlayScene scene; QString error;
    OverlayScene::fromJson(QJsonDocument::fromJson(m_store->value("overlays/scene").toByteArray()).object(), &scene, &error);
    return scene;
}
bool Settings::indicator() const { return m_store->value("overlay/indicator", false).toBool(); }
QString Settings::indicatorPosition() const {
    const auto value = m_store->value("overlay/position", "top-right").toString();
    return QStringList{"top-left", "top-right", "bottom-left", "bottom-right"}.contains(value) ? value : "top-right";
}
QString Settings::accent() const { return m_store->value("appearance/accent", "violet").toString(); }
QStringList Settings::shortcutKeys() const {
    const auto keys = m_store->value("overlay/shortcuts", QStringList{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"}).toStringList();
    return keys.size() == 3 ? keys : QStringList{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"};
}
bool Settings::save(const GsrOptions &o, const OverlayScene &scene, bool indicator, QString *error, const QStringList &keys, const QString &accent, const QString &position) {
    auto &s = *m_store;
    s.setValue("source", o.source); s.setValue("codec", o.codec); s.setValue("container", o.container);
    s.setValue("fps", o.frameRate); s.setValue("bufferSeconds", o.bufferSeconds); s.setValue("bitrateMbps", o.bitrateMbps);
    s.setValue("systemAudio", o.systemAudio); s.setValue("microphone", o.microphone);
    s.setValue("audio/output", o.audioOutput);
    s.setValue("audio/input", o.audioInput);
    s.setValue("audio/codec", o.audioCodec);
    s.setValue("audio/bitrateKbps", o.audioBitrateKbps);
    s.setValue("audio/microphoneTrack", o.microphoneTrack);
    s.setValue("audio/pcTrack", o.pcTrack);
    s.setValue("audio/applicationMode", o.applicationAudio);
    QVariantMap routes;
    for (auto it = o.applicationTracks.cbegin(); it != o.applicationTracks.cend(); ++it) routes.insert(it.key(), it.value());
    s.setValue("audio/applicationTracks", routes);
    s.setValue("audio/systemVolume", o.levels.systemVolume);
    s.setValue("audio/microphoneVolume", o.levels.microphoneVolume);
    s.setValue("audio/microphoneBoost", o.levels.microphoneBoost);
    s.setValue("audio/systemMuted", o.levels.systemMuted);
    s.setValue("audio/microphoneMuted", o.levels.microphoneMuted);
    s.setValue("separateAudioTracks", o.separateAudioTracks); s.setValue("continuousRecording", o.continuousRecording);
    s.setValue("outputDirectory", o.outputDirectory); s.setValue("overlay/indicator", indicator);
    s.setValue("organizeByGame", o.organizeByGame);
    s.setValue("overlay/shortcuts", keys);
    s.setValue("overlay/position", position);
    if (!accent.isEmpty()) s.setValue("appearance/accent", accent);
    s.setValue("overlays/scene", QJsonDocument(scene.toJson()).toJson(QJsonDocument::Compact));
    s.sync();
    if (s.status() != QSettings::NoError) { *error = "Cannot write configuration: " + s.fileName(); return false; }
    error->clear(); return true;
}
} // namespace Shadowless
