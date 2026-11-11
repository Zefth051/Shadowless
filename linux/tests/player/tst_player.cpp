#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QtEndian>
#include "../../player/media_tools.h"
using namespace Shadowless;
class PlayerTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QString source;
    QByteArray original;
    static bool run(const QString &program, const QStringList &args) {
        QProcess p; p.start(program, args);
        return p.waitForFinished(30000) && p.exitCode() == 0;
    }
    static QByteArray bytes(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    static QList<QByteArray> videoPackets(const QString &path) {
        QProcess p; p.start("ffprobe",{"-v","error","-select_streams","v:0","-show_packets","-show_entries","packet=data_hash","-show_data_hash","sha256","-of","csv=p=0",path});
        if(!p.waitForFinished(10000)||p.exitCode()!=0) return {};
        return p.readAllStandardOutput().trimmed().split('\n');
    }
private slots:
    void initTestCase() {
        source = dir.filePath("source.mkv");
        QVERIFY(run("ffmpeg", {"-v","error","-f","lavfi","-i","testsrc2=s=160x90:r=15:d=3",
            "-f","lavfi","-i","sine=frequency=440:duration=3","-f","lavfi","-i","sine=frequency=880:duration=3",
            "-map","0:v","-map","1:a","-map","2:a","-c:v","libx264","-threads","1","-c:a","aac",source}));
        original = bytes(source);
    }
    void analyzesBoundedTimelineAndBothTracks() {
        MediaTools tools; QSignalSpy meta(&tools,&MediaTools::metadataReady), thumbs(&tools,&MediaTools::thumbnailsReady);
        QSignalSpy waves(&tools,&MediaTools::waveformReady), done(&tools,&MediaTools::finished), errors(&tools,&MediaTools::failed);
        QVERIFY(tools.analyze(source)); QVERIFY(!tools.analyze(source));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,30000);
        QCOMPARE(errors.size(),0); QCOMPARE(meta.size(),1);
        const auto info=qvariant_cast<MediaInfo>(meta[0][0]);
        QCOMPARE(info.videoSize,QSize(160,90)); QCOMPARE(info.audioStreams.size(),2);
        QVERIFY(info.durationMs>=3000 && info.durationMs<3200);
        QCOMPARE(info.audioStreams[0].index,1); QCOMPARE(info.audioStreams[1].index,2);
        QCOMPARE(thumbs.size(),1); const auto images=qvariant_cast<QVector<QImage>>(thumbs[0][0]);
        QCOMPARE(images.size(),12); for (const auto &i:images) { QVERIFY(!i.isNull()); QVERIFY(i.width()<=160); QVERIFY(i.height()<=90); }
        QCOMPARE(waves.size(),2);
        for (const auto &w:waves) { const auto peaks=qvariant_cast<QVector<float>>(w[1]); QCOMPARE(peaks.size(),512); QVERIFY(*std::max_element(peaks.begin(),peaks.end())>0.01f); }
    }
    void trimsSelectedStreamsAndMixes() {
        for(bool mix:{false,true}) {
            MediaTools tools; QSignalSpy done(&tools,&MediaTools::finished), ok(&tools,&MediaTools::exported), errors(&tools,&MediaTools::failed);
            ExportRequest r; r.sourcePath=source; r.destinationPath=dir.filePath(mix?"mixed.mp4":"separate.mp4");
            r.inMs=500; r.outMs=2000; r.audioTracks={0,1}; r.mixAudio=mix;
            QVERIFY(tools.exportClip(r)); QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,30000);
            QVERIFY2(errors.isEmpty(),errors.isEmpty()?"":qPrintable(errors[0][0].toString())); QCOMPARE(ok.size(),1);
            QVERIFY(run("ffmpeg",{"-v","error","-i",r.destinationPath,"-f","null","-"}));
            const auto originalPackets=videoPackets(source), outputPackets=videoPackets(r.destinationPath);
            QVERIFY(!outputPackets.isEmpty()); for(const auto &packet:outputPackets) QVERIFY(originalPackets.contains(packet));
            MediaTools check; QSignalSpy metadata(&check,&MediaTools::metadataReady), complete(&check,&MediaTools::finished);
            QVERIFY(check.analyze(r.destinationPath)); QTRY_VERIFY_WITH_TIMEOUT(!metadata.isEmpty(),10000);
            auto info=qvariant_cast<MediaInfo>(metadata[0][0]); QCOMPARE(info.audioStreams.size(),mix?1:2);
            // Stream copy keeps packets from the preceding keyframe (the fixture starts at 0).
            QVERIFY(info.durationMs>=1400 && info.durationMs<=2500); check.stop(); QTRY_COMPARE_WITH_TIMEOUT(complete.size(),1,10000);
        }
        QCOMPARE(bytes(source),original);
    }
    void collisionsCancellationAndFailurePreserveFiles() {
        const QString dest=dir.filePath("existing.mp4"); QFile f(dest); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("existing"); f.close();
        MediaTools tools; QSignalSpy errors(&tools,&MediaTools::failed), done(&tools,&MediaTools::finished), cancelled(&tools,&MediaTools::cancelled);
        ExportRequest r; r.sourcePath=source; r.destinationPath=dest; r.outMs=2000; r.audioTracks={0};
        QVERIFY(tools.exportClip(r)); QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,10000); QCOMPARE(errors.size(),1); QCOMPARE(bytes(dest),QByteArray("existing"));
        r.overwrite=true; QVERIFY(tools.exportClip(r)); tools.stop(); QTRY_COMPARE_WITH_TIMEOUT(done.size(),2,10000);
        QCOMPARE(cancelled.size(),1); QCOMPARE(bytes(dest),QByteArray("existing"));
        r.audioTracks={99}; QVERIFY(tools.exportClip(r)); QTRY_COMPARE_WITH_TIMEOUT(done.size(),3,10000); QCOMPARE(errors.size(),2);
        QCOMPARE(bytes(dest),QByteArray("existing")); QCOMPARE(bytes(source),original);
        QCOMPARE(QDir(dir.path()).entryList({"*.part.*"},QDir::Files|QDir::Hidden).size(),0);
    }
    void switchingFromMetadataDiscardsOldAnalysis() {
        MediaTools tools; QSignalSpy done(&tools,&MediaTools::finished), thumbs(&tools,&MediaTools::thumbnailsReady), errors(&tools,&MediaTools::failed);
        bool switched=false;
        connect(&tools,&MediaTools::metadataReady,&tools,[&] { if(!switched) { switched=true; tools.stop(); QVERIFY(tools.analyze(source)); } });
        QVERIFY(tools.analyze(source)); QTRY_COMPARE_WITH_TIMEOUT(done.size(),2,15000);
        QCOMPARE(errors.size(),0); QCOMPARE(thumbs.size(),1);
    }
    void silentAndSingleTrackExportsAndSafeSourceReplacement() {
        for(int track:{-1,1}) {
            const QString target=dir.filePath(QString("overwrite%1.mkv").arg(track)); QVERIFY(QFile::copy(source,target));
            MediaTools tools; QSignalSpy done(&tools,&MediaTools::finished), ok(&tools,&MediaTools::exported), errors(&tools,&MediaTools::failed);
            ExportRequest r; r.sourcePath=target; r.destinationPath=target; r.inMs=300; r.outMs=1800; r.overwrite=true;
            if(track>=0) r.audioTracks={track};
            QVERIFY(tools.exportClip(r)); QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,15000);
            QCOMPARE(errors.size(),0); QCOMPARE(ok.size(),1); QVERIFY(bytes(target)!=original);
            QVERIFY(run("ffmpeg",{"-v","error","-i",target,"-f","null","-"}));
            MediaTools check; QSignalSpy meta(&check,&MediaTools::metadataReady), waves(&check,&MediaTools::waveformReady), complete(&check,&MediaTools::finished);
            QVERIFY(check.analyze(target)); QTRY_COMPARE_WITH_TIMEOUT(complete.size(),1,15000);
            QCOMPARE(qvariant_cast<MediaInfo>(meta[0][0]).audioStreams.size(),track<0?0:1); QCOMPARE(waves.size(),track<0?0:1);
            if(track==1) {
                QProcess pcm; pcm.start("ffmpeg",{"-v","error","-i",target,"-map","0:a:0","-ac","1","-ar","44100","-f","s16le","pipe:1"});
                QVERIFY(pcm.waitForFinished(10000)); QCOMPARE(pcm.exitCode(),0);
                const auto data=pcm.readAllStandardOutput(); int crossings=0; qint16 previous=0;
                for(int i=0;i+1<data.size();i+=2) {
                    const auto sample=qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(data.constData()+i));
                    if(previous<0 && sample>=0) ++crossings;
                    previous=sample;
                }
                const double hz=crossings/(data.size()/2.0/44100);
                QVERIFY2(hz>800 && hz<950,qPrintable(QString::number(hz)));
            }
        }
    }
    void activeCancellationAndEncoderFailurePreserveDestination() {
        const QString destination=dir.filePath("preserved.mkv"), damaged=dir.filePath("damaged.mkv");
        QVERIFY(QFile::copy(source,damaged)); QFile f(destination); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("preserve me"); f.close();
        for(bool cancel:{true,false}) {
            MediaTools tools; QSignalSpy done(&tools,&MediaTools::finished), errors(&tools,&MediaTools::failed), cancelled(&tools,&MediaTools::cancelled);
            connect(&tools,&MediaTools::progress,&tools,[&](double progress) {
                if(progress!=0) return;
                if(cancel) QTimer::singleShot(0,&tools,&MediaTools::stop);
                else { QFile clip(damaged); QVERIFY(clip.open(QIODevice::WriteOnly)); clip.close(); }
            });
            ExportRequest r; r.sourcePath=cancel?source:damaged; r.destinationPath=destination; r.outMs=2000; r.overwrite=true;
            QVERIFY(tools.exportClip(r)); QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,10000);
            QCOMPARE(cancelled.size(),cancel?1:0); QCOMPARE(errors.size(),cancel?0:1);
            QCOMPARE(bytes(destination),QByteArray("preserve me")); QCOMPARE(bytes(source),original);
        }
        QCOMPARE(QDir(dir.path()).entryList({"*.part.*"},QDir::Files|QDir::Hidden).size(),0);
    }
};
QTEST_GUILESS_MAIN(PlayerTests)
#include "tst_player.moc"
