#include "media_tools.h"
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QTimer>
#include <QtEndian>
#include <cmath>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/resource.h>

namespace Shadowless {
namespace {
QString seconds(qint64 ms) { return QString::number(ms / 1000.0, 'f', 3); }
QStringList ffmpegArgs() { return {"-v", "error", "-nostdin", "-threads", "1", "-filter_threads", "1", "-filter_complex_threads", "1"}; }
}
MediaTools::MediaTools(QObject *p):QObject(p),m_process(new QProcess) {
    qRegisterMetaType<MediaInfo>(); qRegisterMetaType<QVector<QImage>>(); qRegisterMetaType<QVector<float>>();
    m_process->setChildProcessModifier([] { setpriority(PRIO_PROCESS,0,15); });
    connect(m_process,&QProcess::readyReadStandardOutput,this,&MediaTools::consume);
    connect(m_process,&QProcess::readyReadStandardError,this,[this] {
        m_errors += m_process->readAllStandardError();
        if(m_errors.size()>16384) m_errors=m_errors.right(16384);
    });
    connect(m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e) {
        if(e==QProcess::FailedToStart && m_busy) fail(m_process->errorString());
    });
    connect(m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status) {
        consume();
        if(!m_busy) return;
        if(m_stopping) { cleanup(); m_busy=false; m_stopping=false; emit cancelled(); emit finished(); return; }
        if(code || status!=QProcess::NormalExit) { fail(QString::fromUtf8(m_errors).trimmed().left(2000)); return; }
        auto done=std::move(m_done); if(done) done();
    });
}
MediaTools::~MediaTools() {
    disconnect(m_process,nullptr,this,nullptr);
    if(m_process->state()!=QProcess::NotRunning) {
        connect(m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),m_process,&QObject::deleteLater);
        m_process->kill();
    } else delete m_process;
    cleanup();
}
void MediaTools::cleanup() { if(!m_tempPath.isEmpty()) { QFile::remove(m_tempPath); m_tempPath.clear(); } m_done={}; }
void MediaTools::finish() { cleanup(); m_busy=false; emit finished(); }
void MediaTools::fail(const QString &message) { cleanup(); m_busy=false; emit failed(message.isEmpty()?QStringLiteral("Media processing failed."):message); emit finished(); }
void MediaTools::stop() {
    if(!m_busy) return;
    ++m_generation; m_stopping=true;
    if(m_process->state()!=QProcess::NotRunning) m_process->kill();
    else { cleanup(); m_busy=false; m_stopping=false; emit cancelled(); emit finished(); }
}
void MediaTools::consume() {
    const auto data=m_process->readAllStandardOutput();
    if(!m_waveform) {
        if(m_output.size()+data.size()>2*1024*1024) { m_errors="Media analysis output exceeded its limit."; m_process->kill(); }
        else m_output+=data;
        return;
    }
    m_pcm+=data;
    const qint64 total=qMax<qint64>(1,m_info.durationMs/10);
    int consumed=0;
    while(consumed+1<m_pcm.size()) {
        const auto sample=qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(m_pcm.constData()+consumed));
        const int bin=int(qMin<qint64>(511,m_sample*512/total));
        m_peaks[bin]=qMax(m_peaks[bin],float(std::abs(int(sample))/32768.0));
        ++m_sample; consumed+=2;
    }
    m_pcm.remove(0,consumed);
}
void MediaTools::launch(const QString &program,const QStringList &args,std::function<void()> done,bool waveform) {
    if(!m_busy || m_stopping) return;
    m_output.clear(); m_errors.clear(); m_pcm.clear(); m_waveform=waveform; m_done=std::move(done);
    m_process->start(program,args);
}
void MediaTools::probe(const QString &path,std::function<void(MediaInfo)> done) {
    launch("ffprobe",{"-v","error","-threads","1","-show_entries","format=duration:stream=index,codec_type,codec_name,width,height,channels:stream_tags=title","-of","json",path},[this,path,done] {
        QJsonParseError error; auto document=QJsonDocument::fromJson(m_output,&error);
        if(error.error!=QJsonParseError::NoError) { fail("Invalid media metadata."); return; }
        const auto object=document.object(); MediaInfo info; info.path=path;
        const double duration=object["format"].toObject()["duration"].toString().toDouble();
        if(!std::isfinite(duration) || duration<=0 || duration>86400*365) { fail("Unsupported media duration."); return; }
        info.durationMs=qRound64(duration*1000);
        for(const auto &v:object["streams"].toArray()) {
            const auto s=v.toObject();
            if(s["codec_type"]=="video" && info.videoSize.isEmpty()) info.videoSize=QSize(s["width"].toInt(),s["height"].toInt());
            if(s["codec_type"]=="audio") info.audioStreams.append({s["index"].toInt(),s["tags"].toObject()["title"].toString(),s["codec_name"].toString(),s["channels"].toInt()});
        }
        if(info.videoSize.isEmpty() || info.audioStreams.size()>32) { fail("Unsupported video or audio stream count."); return; }
        done(info);
    });
}
bool MediaTools::analyze(const QString &path) {
    if(m_busy) return false;
    m_busy=true; m_stopping=false; m_thumbnail=0; m_audio=0; m_thumbnails.clear();
    const auto generation=++m_generation;
    QTimer::singleShot(0,this,[this,path,generation] {
        if(!m_busy || generation!=m_generation) return;
        if(!QFileInfo(path).isFile()) { fail("The source clip does not exist."); return; }
        probe(QFileInfo(path).absoluteFilePath(),[this,generation](MediaInfo info) {
            m_info=info; emit metadataReady(info); if(m_busy && generation==m_generation) nextThumbnail();
        });
    });
    return true;
}
void MediaTools::nextThumbnail() {
    const auto generation=m_generation;
    if(m_thumbnail==12) { emit thumbnailsReady(m_thumbnails); if(m_busy && generation==m_generation) nextWaveform(); return; }
    auto args=ffmpegArgs();
    args << "-ss" << seconds(m_info.durationMs*m_thumbnail/12) << "-i" << m_info.path
         << "-map" << "0:v:0" << "-an" << "-frames:v" << "1" << "-vf"
         << "scale=160:90:force_original_aspect_ratio=decrease,pad=160:90:(ow-iw)/2:(oh-ih)/2" << "-threads" << "1" << "-f" << "image2pipe" << "-vcodec" << "png" << "pipe:1";
    launch("ffmpeg",args,[this,generation] {
        const auto image=QImage::fromData(m_output,"PNG");
        if(image.isNull()) { fail("Could not decode the timeline thumbnail."); return; }
        m_thumbnails.append(image); ++m_thumbnail; emit progress(m_thumbnail/24.0); if(m_busy && generation==m_generation) nextThumbnail();
    });
}
void MediaTools::nextWaveform() {
    const auto generation=m_generation;
    if(m_audio==m_info.audioStreams.size()) { emit progress(1); if(m_busy && generation==m_generation) finish(); return; }
    m_peaks.fill(0,512); m_sample=0;
    auto args=ffmpegArgs(); args << "-i" << m_info.path << "-map" << QString("0:a:%1").arg(m_audio)
        << "-vn" << "-af" << "aformat=channel_layouts=mono,aeval=abs(val(0)),aresample=100" << "-ac" << "1" << "-ar" << "100" << "-f" << "s16le" << "pipe:1";
    launch("ffmpeg",args,[this,generation] {
        emit waveformReady(m_audio,m_peaks); if(!m_busy || generation!=m_generation) return;
        ++m_audio; emit progress(0.5+0.5*m_audio/qMax(1,m_info.audioStreams.size()));
        if(m_busy && generation==m_generation) nextWaveform();
    },true);
}
bool MediaTools::exportClip(const ExportRequest &request) {
    if(m_busy) return false;
    m_busy=true; m_stopping=false;
    const auto generation=++m_generation;
    QTimer::singleShot(0,this,[this,request,generation] {
        if(!m_busy || generation!=m_generation) return;
        ExportRequest r=request; r.sourcePath=QFileInfo(r.sourcePath).absoluteFilePath(); r.destinationPath=QFileInfo(r.destinationPath).absoluteFilePath();
        const QFileInfo dest(r.destinationPath);
        if(!QFileInfo(r.sourcePath).isFile() || dest.isSymLink() || (dest.exists()&&!r.overwrite) || !dest.dir().exists()) { fail("The output exists or the source/output path is invalid."); return; }
        if(dest.suffix().toLower()!="mp4" && dest.suffix().toLower()!="mkv") { fail("Export requires an MP4 or MKV output path."); return; }
        probe(r.sourcePath,[this,r,generation](MediaInfo info) {
            if(r.inMs<0 || r.outMs<=r.inMs || r.outMs>info.durationMs) { fail("The trim range is invalid."); return; }
            QVector<int> seen;
            for(int ordinal:r.audioTracks) { if(ordinal<0 || ordinal>=info.audioStreams.size() || seen.contains(ordinal)) { fail("The selected audio track is invalid."); return; } seen.append(ordinal); }
            QTemporaryFile temp(QFileInfo(r.destinationPath).dir().filePath(".shadowless-XXXXXX.part."+QFileInfo(r.destinationPath).suffix()));
            if(!temp.open()) { fail("Cannot create a temporary export beside the destination."); return; }
            temp.setAutoRemove(false); m_tempPath=temp.fileName(); temp.close();
            auto args=ffmpegArgs(); args << "-y" << "-ss" << seconds(r.inMs) << "-i" << r.sourcePath << "-t" << seconds(r.outMs-r.inMs) << "-map" << "0:v:0";
            if(r.mixAudio && r.audioTracks.size()>1) {
                QString graph;
                for(int i:r.audioTracks) graph+=QString("[0:a:%1]").arg(i);
                graph+=QString("amix=inputs=%1:duration=longest:normalize=1[mix]").arg(r.audioTracks.size());
                args << "-filter_complex" << graph << "-map" << "[mix]";
            } else for(int i:r.audioTracks) args << "-map" << QString("0:a:%1").arg(i);
            // Video packets are copied to protect capture; the beginning can include keyframe preroll.
            args << "-c:v" << "copy" << "-threads" << "1" << "-c:a"
                 << ((r.mixAudio && r.audioTracks.size()>1)?"aac":"copy") << m_tempPath;
            emit progress(0);
            if(!m_busy || generation!=m_generation) return;
            launch("ffmpeg",args,[this,r] {
                probe(m_tempPath,[this,r](MediaInfo info) {
                    const int tracks=(r.mixAudio&&!r.audioTracks.isEmpty())?1:r.audioTracks.size();
                    // Copied video can retain preroll back to the first source keyframe.
                    if(info.audioStreams.size()!=tracks || info.durationMs<qMax<qint64>(1,r.outMs-r.inMs-1000) || info.durationMs>r.outMs+1000) { fail("The exported clip failed validation."); return; }
                    auto args=ffmpegArgs(); args << "-xerror" << "-i" << m_tempPath << "-map" << "0:v" << "-map" << "0:a?" << "-f" << "null" << "-";
                    launch("ffmpeg",args,[this,r] {
                        const auto temp=QFile::encodeName(m_tempPath), dest=QFile::encodeName(r.destinationPath);
                        // link() atomically refuses collisions; rename() atomically replaces only after validation.
                        const int result=r.overwrite?::rename(temp.constData(),dest.constData()) : ::link(temp.constData(),dest.constData());
                        if(result!=0) { fail(QString("Cannot install exported clip: %1").arg(QString::fromLocal8Bit(std::strerror(errno)))); return; }
                        if(!r.overwrite) QFile::remove(m_tempPath);
                        m_tempPath.clear(); cleanup(); m_busy=false; emit progress(1); emit exported(r.destinationPath); emit finished();
                    });
                });
            });
        });
    });
    return true;
}
}
