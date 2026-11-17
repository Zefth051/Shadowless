#include "idle_compressor.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtConcurrent>
#include <cmath>
#include <filesystem>

namespace Shadowless {
namespace {
struct Probe { bool ok = false; QString video; int videos = 0, audios = 0, width = 0, height = 0; double duration = 0, fps = 0; };
bool isTemp(const QString &name) { return name.contains(".shadowless-tmp"); }
QString host(const QString &tool) { return QStandardPaths::findExecutable(tool); }
Probe probe(const QString &path) {
    Probe result; QProcess p;
    p.start(host("ffprobe"), {"-v", "error", "-show_entries", "stream=codec_type,codec_name,width,height,avg_frame_rate:format=duration", "-of", "json", path});
    if (!p.waitForFinished(30000) || p.exitCode() != 0) return result;
    const auto root = QJsonDocument::fromJson(p.readAllStandardOutput()).object();
    for (const auto &value : root["streams"].toArray()) {
        const auto stream = value.toObject();
        if (stream["codec_type"] == "video") { if (!result.videos++) {
                result.video = stream["codec_name"].toString(); result.width = stream["width"].toInt(); result.height = stream["height"].toInt();
                const auto rate = stream["avg_frame_rate"].toString().split('/');
                if (rate.size() == 2 && rate[1].toDouble() > 0) result.fps = rate[0].toDouble() / rate[1].toDouble();
            } }
        else if (stream["codec_type"] == "audio") ++result.audios;
    }
    result.duration = root["format"].toObject()["duration"].toString().toDouble();
    result.ok = result.videos > 0 && result.duration > 0;
    return result;
}
} // namespace

IdleCompressor::IdleCompressor(QObject *parent) : QObject(parent) {
    connect(&m_watcher, &QFutureWatcher<qint64>::finished, this, [this] {
        const auto saved = m_watcher.result();
        if (saved < 0) m_done.remove(m_current); // cancelled: retry in the next idle window
        else if (saved > 0) emit compressed(m_current, saved);
        m_current.clear(); next();
    });
}
IdleCompressor::~IdleCompressor() { m_cancel = true; m_watcher.waitForFinished(); }
void IdleCompressor::setEnabled(bool enabled) { m_enabled = enabled; if (!enabled) m_cancel = true; else next(); }
void IdleCompressor::setRoot(const QString &root) { m_root = root; }
void IdleCompressor::setBusy(bool busy) {
    m_busy = busy;
    if (busy) m_cancel = true; else next();
}
void IdleCompressor::next() {
    if (!m_enabled || m_busy || m_root.isEmpty() || m_watcher.isRunning()) return;
    for (const auto &path : candidates(m_root, QDateTime::currentDateTime())) {
        if (m_done.contains(path)) continue;
        m_done.insert(path); m_current = path; m_cancel = false;
        m_watcher.setFuture(QtConcurrent::run([this, path] {
            QString detail; const auto before = QFileInfo(path).size();
            const auto result = compressFile(path, m_cancel, &detail);
            if (result == Result::Cancelled) return qint64(-1);
            return result == Result::Compressed ? before - QFileInfo(path).size() : qint64(0);
        }));
        return;
    }
}
QStringList IdleCompressor::candidates(const QString &root, const QDateTime &now, int minimumAgeSeconds) {
    QStringList found;
    QDirIterator it(root, {"*.mp4"}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (isTemp(info.fileName()) || info.fileName().startsWith('.')) continue;
        if (info.lastModified().secsTo(now) < minimumAgeSeconds) continue;
        found << info.absoluteFilePath();
    }
    found.sort();
    return found;
}
QStringList IdleCompressor::ffmpegArguments(const QString &input, const QString &output) {
    return {"-nostdin", "-y", "-v", "error", "-i", input, "-map", "0:v", "-map", "0:a?", "-map_metadata", "0",
            "-c", "copy", "-c:v", "libx265", "-crf", "19", "-preset", "slow", "-tag:v", "hvc1",
            "-movflags", "+faststart", "-f", "mp4", output};
}
bool IdleCompressor::validate(const QString &original, const QString &compressed, QString *error) {
    const auto a = probe(original), b = probe(compressed);
    auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    if (!a.ok || !b.ok) return fail("ffprobe could not read the clip");
    if (b.video != "hevc") return fail("compressed video is not HEVC");
    if (a.videos != b.videos || a.audios != b.audios) return fail("stream count changed");
    if (std::abs(a.duration - b.duration) > 0.5) return fail("duration changed");
    if (std::abs(a.fps - b.fps) > 0.01 || a.width != b.width || a.height != b.height) return fail("frame rate or size changed");
    // Perceptual gate: the clip is replaced only if it is visually indistinguishable from the original.
    QProcess ssim;
    ssim.start(host("ffmpeg"), {"-nostdin", "-v", "info", "-i", compressed, "-i", original, "-lavfi", "[0:v:0][1:v:0]ssim", "-f", "null", "-"});
    if (!ssim.waitForFinished(600000) || ssim.exitCode() != 0) return fail("quality check failed");
    const auto match = QRegularExpression(R"(All:([0-9.]+))").match(QString::fromUtf8(ssim.readAllStandardError()));
    if (!match.hasMatch() || match.captured(1).toDouble() < 0.985) return fail("quality below threshold");
    return true;
}
IdleCompressor::Result IdleCompressor::compressFile(const QString &path, std::atomic_bool &cancel, QString *detail) {
    auto note = [detail](const QString &text) { if (detail) *detail = text; };
    if (cancel) { note("cancelled"); return Result::Cancelled; }
    const auto ffmpeg = host("ffmpeg"), nice = host("nice");
    if (ffmpeg.isEmpty() || host("ffprobe").isEmpty()) { note("ffmpeg/ffprobe not found"); return Result::Skipped; }
    const auto source = probe(path);
    if (!source.ok) { note("unreadable"); return Result::Failed; }
    if (source.video != "h264") { note("already compressed"); return Result::Skipped; }
    const QFileInfo info(path);
    const auto temp = info.absolutePath() + "/." + info.completeBaseName() + ".shadowless-tmp.mp4";
    QFile::remove(temp);
    QProcess process;
    process.setProgram(nice.isEmpty() ? ffmpeg : nice);
    const auto args = ffmpegArguments(path, temp);
    process.setArguments(nice.isEmpty() ? args : QStringList{"-n", "19", ffmpeg} + args);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.start();
    while (!process.waitForFinished(500)) {
        if (cancel) { process.kill(); process.waitForFinished(5000); QFile::remove(temp); note("cancelled"); return Result::Cancelled; }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QFile::remove(temp); note(QString::fromUtf8(process.readAllStandardError()).trimmed()); return Result::Failed;
    }
    QString error;
    if (!validate(path, temp, &error)) { QFile::remove(temp); note(error); return Result::Failed; }
    if (QFileInfo(temp).size() >= qint64(QFileInfo(path).size() * 0.9)) { QFile::remove(temp); note("no useful size reduction"); return Result::Skipped; }
    QFile tempFile(temp);
    if (tempFile.open(QIODevice::ReadWrite)) { tempFile.setFileTime(info.lastModified(), QFileDevice::FileModificationTime); tempFile.close(); }
    std::error_code ec;
    std::filesystem::rename(temp.toStdString(), path.toStdString(), ec); // atomic replace within one directory
    if (ec) { QFile::remove(temp); note(QString::fromStdString(ec.message())); return Result::Failed; }
    return Result::Compressed;
}
} // namespace Shadowless
