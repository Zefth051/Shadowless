#include "../capture/gsr_capture.h"
#include "../capture/audio.h"
#include "../capture/replay_paths.h"
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalServer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QFileDevice>
#include <QScopeGuard>

using namespace Shadowless;

class GsrTests : public QObject {
    Q_OBJECT
    const GsrCapabilities supported{"6.1.5", "nvidia", "wayland", {"h264", "hevc"}, {"DP-1", "portal"}};
private slots:
    void recordingFailureMarkerClearsRecordingWithoutStoppingReplay() {
        QTemporaryDir directory; QFile script(directory.filePath("recorder.py"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/usr/bin/env python3\nimport time\n"
            "print('ordinary Started recording diagnostic',flush=True);time.sleep(.1)\n"
            "print('gsr info: Started recording',flush=True);time.sleep(.25)\n"
            "print('gsr error: Failed to save recording',flush=True);time.sleep(.4)\n");
        script.close(); QVERIFY(script.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
        GsrCapture engine(nullptr,script.fileName()); GsrOptions options;
        options.source="DP-1"; options.systemAudio=false; options.outputDirectory=directory.path();
        QSignalSpy changed(&engine,&GsrCapture::recordingChanged);
        engine.start(options,supported);
        QTRY_VERIFY(engine.replayRecording());
        QCOMPARE(changed.size(),1); QCOMPARE(changed[0][0].toBool(),true);
        QTRY_VERIFY(!engine.replayRecording());
        QVERIFY(engine.active()); QCOMPARE(changed.size(),2); QCOMPARE(changed[1][0].toBool(),false);
        QTRY_VERIFY(!engine.active());
    }
    void finalizerProgressRenewsPendingCommandTimeout() {
        GsrRequest request; QSignalSpy finished(&request,&GsrRequest::finished);
        auto *timer=request.findChild<QTimer *>(); QVERIFY(timer);
        timer->start(10); request.keepAlive(200);
        QTest::qWait(40); QCOMPARE(finished.size(),0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,1000);
        QVERIFY(!finished[0][0].toBool());
        request.keepAlive(1); QTest::qWait(10); QCOMPARE(finished.size(),1);
    }
    void automaticAudioKeepsMicrophoneIndependentFromOldMixSettings() {
        GsrOptions options; options.microphone=true; options.separateAudioTracks=false;
        QVERIFY(options.dynamicAudioEnabled());
        options.audioOutput="old-unplugged-output.monitor";
        QCOMPARE(options.audioTracks(),QStringList({"name:PC|app-inverse:","name:Microphone|default_input"}));
        options.applicationAudio=true; QVERIFY(!options.dynamicAudioEnabled());
        options.applicationAudio=false; options.fourTrackAudio=true; QVERIFY(!options.dynamicAudioEnabled());
        options.fourTrackAudio=false; options.systemAudio=false; QVERIFY(!options.dynamicAudioEnabled());
        QVERIFY(isKnownAudioGame("Rocket League")); QVERIFY(isKnownAudioGame("cs2"));
        QVERIFY(!isKnownAudioGame("Spotify"));
    }
    void dynamicApplicationsArriveWithoutRestartingCapture() {
        QTemporaryDir directory;
        QFile script(directory.filePath("recorder.py")); QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/usr/bin/env python3\nimport os,time,json\n"
            "assert os.environ['SHADOWLESS_DYNAMIC_AUDIO']=='1'\n"
            "print('gsr info: shadowless-audio-app: '+json.dumps({'name':'Spotify','game':False}),flush=True)\n"
            "time.sleep(.15)\n"
            "print('gsr info: shadowless-audio-app: bad JSON',flush=True)\n"
            "print('gsr info: shadowless-audio-app: '+json.dumps({'name':'Late browser','game':False}),flush=True)\n"
            "time.sleep(1)\n");
        script.close(); QVERIFY(script.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
        GsrOptions options; options.source="DP-1"; options.outputDirectory=directory.path();
        options.dynamicApplicationAudio=true;
        GsrCapture engine(nullptr,script.fileName());
        QSignalSpy apps(&engine,&GsrCapture::applicationAudioDiscovered), stopped(&engine,&GsrCapture::stopped);
        engine.start(options,supported);
        QTRY_COMPARE(apps.size(),1); QCOMPARE(apps[0][0].toString(),QString("Spotify"));
        QVERIFY(engine.setApplicationAudioState("Spotify",false,false));
        QFile state(engine.applicationAudioStatePath()); QVERIFY(state.open(QIODevice::ReadOnly));
        const auto selections=QJsonDocument::fromJson(state.readAll()).object();
        QVERIFY(!selections["Spotify"].toObject()["included"].toBool());
        QTRY_COMPARE(apps.size(),2); QCOMPARE(apps[1][0].toString(),QString("Late browser"));
        QCOMPARE(stopped.size(),0); QCOMPARE(engine.applicationAudioSources().size(),2);
        QVERIFY(!engine.setApplicationAudioState(QString(),true,true));
        QVERIFY(engine.setApplicationAudioState("Late browser",true,true));
        QTRY_VERIFY(!engine.active());
    }
    void roleVolumeValidationRejectsOutOfRange() {
        GsrOptions options; options.source="DP-1"; options.outputDirectory="/tmp/clips";
        for (int invalid : {-1, 101}) {
            options.levels.gameVolume=invalid;
            QCOMPARE(options.validate(supported), QString("Audio levels are out of range."));
            options.levels.gameVolume=100; options.levels.musicVolume=invalid;
            QCOMPARE(options.validate(supported), QString("Audio levels are out of range."));
            options.levels.musicVolume=100;
        }
        options.levels.gameVolume=0; options.levels.musicVolume=100;
        QVERIFY(options.validate(supported).isEmpty());
    }
    void directAudioUsesOnlyPrivateGainFile() {
        const auto oldPath = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", oldPath); });
        qputenv("PATH", "/nonexistent");
        CaptureAudio audio;
        QSignalSpy ready(&audio, &CaptureAudio::ready), failed(&audio, &CaptureAudio::failed);
        GsrOptions options; options.microphone = true; options.applicationAudio = true;
        options.levels.systemVolume = 25; options.levels.microphoneVolume = 50;
        options.levels.microphoneBoost = 20;
        audio.start(options);
        QCOMPARE(ready.size(), 1); QVERIFY(failed.isEmpty());
        QCOMPARE(ready[0][0].toString(), QString()); QCOMPARE(ready[0][1].toString(), QString());
        QFile gains(audio.levelsPath()); QVERIFY(gains.open(QIODevice::ReadOnly));
        QCOMPARE(gains.readAll().trimmed(), QByteArray("0.25 5 1 1")); gains.close();
        auto levels = options.levels; levels.systemMuted = true; levels.microphoneMuted = true;
        levels.gameVolume = 40; levels.musicVolume = 30; levels.musicMuted = true;
        audio.setLevels(levels);
        QVERIFY(gains.open(QIODevice::ReadOnly)); QCOMPARE(gains.readAll().trimmed(), QByteArray("0 0 0.4 0"));
        gains.close(); const auto path = audio.levelsPath(); audio.stop(); QVERIFY(!QFile::exists(path));
    }
    void recordingPauseFollowsRecorderMarkers() {
        QTemporaryDir directory;
        QFile script(directory.filePath("recorder.py")); QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/usr/bin/env python3\nimport time\n"
                     "print('ordinary Paused diagnostic',flush=True);time.sleep(.1)\n"
                     "print('gsr info: Pau',end='',flush=True);time.sleep(.1)\n"
                     "print('sed',flush=True);time.sleep(.2)\n"
                     "print('gsr info: Unpaused',flush=True);time.sleep(.2)\n"
                     "print('gsr info: Paused',flush=True);time.sleep(.2)\n");
        script.close(); QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        GsrOptions options; options.source="DP-1"; options.systemAudio=false;
        options.outputDirectory=directory.path(); options.continuousRecording=true;
        GsrCapture engine(nullptr,script.fileName());
        QSignalSpy changes(&engine,&GsrCapture::recordingPauseChanged), stopped(&engine,&GsrCapture::stopped);
        engine.start(options,supported);
        QTRY_COMPARE(changes.size(),1); QVERIFY(engine.recordingPaused());
        QTRY_COMPARE(changes.size(),2); QVERIFY(!engine.recordingPaused());
        QTRY_COMPARE(changes.size(),3); QVERIFY(engine.recordingPaused());
        QTRY_COMPARE(stopped.size(),1); QCOMPARE(changes.size(),4);
        QVERIFY(!engine.recordingPaused()); QVERIFY(engine.recordingPauseKnown()); QVERIFY(!engine.busy());
    }
    void recordingPauseFollowsEngineConfirmation_data() {
        QTest::addColumn<QString>("behavior");
        QTest::newRow("confirmed") << QString("confirmed");
        QTest::newRow("rejected") << QString("rejected");
        QTest::newRow("unconfirmed") << QString("unconfirmed");
        QTest::newRow("replay-not-supported") << QString("replay");
    }
    void recordingPauseFollowsEngineConfirmation() {
        QFETCH(QString, behavior);
        QTemporaryDir directory;
        QFile script(directory.filePath("recorder.py")); QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/usr/bin/env python3\nbehavior='" + behavior.toUtf8() + "'\n");
        script.write(R"PY(import sys,socket,json,pathlib,time
args=sys.argv
server=socket.socket(socket.AF_UNIX);server.bind(args[args.index('-ipc')+1]);server.listen()
output=pathlib.Path(args[args.index('-o')+1])
if behavior!='replay': output.write_bytes(b'recording')
while True:
    peer,_=server.accept();data=peer.makefile('rb').readline()
    if not data: peer.close();continue
    msg=json.loads(data);name=msg['name']
    with open(__file__+'.commands','a') as log: log.write(json.dumps(msg)+'\n')
    result='error' if name=='set-paused' and behavior=='rejected' else 'ok'
    value='Pause denied' if result=='error' else str(output) if name=='stop' else ''
    peer.sendall((json.dumps({'id':msg['id'],'result':result,'data':value})+'\n').encode());peer.close()
    if name=='set-paused' and behavior=='confirmed':
        assert isinstance(msg['data'],bool)
        time.sleep(.25)
        print('gsr info: Paused' if msg['data'] else 'gsr info: Unpaused',flush=True)
    if name=='stop': break
)PY");
        script.close(); QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        GsrOptions options; options.source="DP-1"; options.systemAudio=false;
        options.outputDirectory=directory.path(); options.continuousRecording=behavior!="replay";
        GsrCapture engine(nullptr,script.fileName());
        QSignalSpy started(&engine,&GsrCapture::started), stopped(&engine,&GsrCapture::stopped);
        QSignalSpy errors(&engine,&GsrCapture::error), clips(&engine,&GsrCapture::clipSaved);
        engine.start(options,supported); QTRY_VERIFY(!started.isEmpty() || !errors.isEmpty());
        QVERIFY2(started.size()==1,errors.isEmpty()?"Recorder did not start":qPrintable(errors[0][0].toString()));
        engine.setRecordingPaused(true);
        QVERIFY(!engine.recordingPaused());
        if(behavior=="confirmed") {
            QVERIFY(engine.busy());
            QTRY_VERIFY(engine.recordingPaused()); QTRY_VERIFY(!engine.busy());
            engine.setRecordingPaused(false);
            QVERIFY(engine.recordingPaused());
            QTRY_VERIFY(!engine.recordingPaused()); QTRY_VERIFY(!engine.busy());
            engine.setRecordingPaused(true); QTRY_VERIFY(engine.recordingPaused()); QTRY_VERIFY(!engine.busy());
            QVERIFY(errors.isEmpty());
        } else {
            QTRY_COMPARE_WITH_TIMEOUT(errors.size(),1,6000);
            QVERIFY(!engine.recordingPaused()); QVERIFY(!engine.busy());
            if(behavior=="unconfirmed") QVERIFY(!engine.recordingPauseKnown());
        }
        engine.stop(); QTRY_COMPARE(stopped.size(),1);
        QVERIFY(!engine.recordingPaused()); QVERIFY(engine.recordingPauseKnown());
        if(behavior!="replay") { QCOMPARE(clips.size(),1); QCOMPARE(clips[0][1].toBool(),true); }
        QFile commands(script.fileName()+".commands"); QVERIFY(commands.open(QIODevice::ReadOnly));
        const auto messages=commands.readAll();
        QCOMPARE(messages.count("set-paused"),behavior=="confirmed"?3:behavior=="replay"?0:1);
        QVERIFY(!messages.contains("start-replay-recording"));
    }
    void authenticationFollowsRecorderMarkers_data() {
        QTest::addColumn<bool>("authorize");
        QTest::addColumn<bool>("complete");
        QTest::newRow("polkit-cancelled") << true << false;
        QTest::newRow("polkit-accepted") << true << true;
        QTest::newRow("no-polkit") << false << false;
    }
    void authenticationFollowsRecorderMarkers() {
        QFETCH(bool, authorize);
        QFETCH(bool, complete);
        QTemporaryDir directory;
        QFile script(directory.filePath("recorder.py")); QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/usr/bin/python\nimport sys,time\nprint('ordinary diagnostic', flush=True)\n");
        if (authorize) script.write(
            "sys.stdout.write('gsr info: shadowless-kms-auth: be');sys.stdout.flush();time.sleep(.1)\n"
            "print('gin', flush=True);time.sleep(.1)\n"
            "print('gsr info: shadowless-kms-auth: begin', flush=True);time.sleep(.1)\n");
        if (complete) script.write("print('gsr info: shadowless-kms-auth: end', flush=True);time.sleep(.5)\n");
        script.write("time.sleep(.2)\nsys.exit(126)\n"); script.close();
        QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        GsrCapture engine(nullptr, script.fileName());
        QSignalSpy auth(&engine, SIGNAL(authenticationChanged(bool))); QVERIFY(auth.isValid());
        QSignalSpy stopped(&engine, &GsrCapture::stopped);
        GsrOptions options; options.source = "DP-1"; options.systemAudio = false; options.outputDirectory = directory.path();
        engine.start(options, supported);
        if (complete) { QTRY_COMPARE(auth.size(), 2); QVERIFY(engine.active()); QCOMPARE(stopped.size(), 0); }
        QTRY_COMPARE(stopped.size(), 1);
        QCOMPARE(auth.size(), authorize ? 2 : 0);
        if (authorize) { QCOMPARE(auth[0][0].toBool(), true); QCOMPARE(auth[1][0].toBool(), false); }
    }
    void audioCatalogPreservesNamesAndAddsIcons() {
        QTemporaryDir directory;
        const auto previousPath = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", previousPath); });
        QFile recorder(directory.filePath("recorder")); QVERIFY(recorder.open(QIODevice::WriteOnly));
        recorder.write("#!/bin/sh\ncase \"$1\" in\n--list-audio-devices) echo 'studio.monitor|Studio' ;;\n--list-application-audio) printf 'Browser\\nMusic\\nUnknown\\nBrowser\\n' ;;\nesac\n");
        recorder.close(); QVERIFY(recorder.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QFile pactl(directory.filePath("pactl")); QVERIFY(pactl.open(QIODevice::WriteOnly));
        pactl.write(R"SH(#!/bin/sh
if [ "$*" != '-f json list sink-inputs' ]; then exit 1; fi
echo '[{"properties":{"application.name":"Browser","application.icon_name":"firefox","application.process.binary":"browser-bin"}},
{"properties":{"application.name":"Browser","application.process.binary":"browser-bin"}},
{"properties":{"application.name":"Music","application.process.binary":"/usr/bin/music-player"}},
{"properties":{"application.name":"Not listed","application.icon_name":"other"}},
{"properties":{"application.name":"Unknown"}}, {"properties":{}}]'
)SH");
        pactl.close(); QVERIFY(pactl.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("PATH", directory.path().toUtf8());
        auto catalog = AudioCatalog::probe(recorder.fileName());
        QCOMPARE(catalog.applications, QStringList({"Browser", "Music", "Unknown"}));
        QCOMPARE(catalog.applicationIcons.value("Browser"), QString("firefox"));
        QCOMPARE(catalog.applicationIcons.value("Music"), QString("music-player"));
        QVERIFY(!catalog.applicationIcons.contains("Unknown"));
        QVERIFY(!catalog.applicationIcons.contains("Not listed"));
        QVERIFY(catalog.error.isEmpty());
        QVERIFY(pactl.open(QIODevice::WriteOnly | QIODevice::Truncate));
        pactl.write("#!/bin/sh\necho 'not json'\n"); pactl.close();
        catalog = AudioCatalog::probe(recorder.fileName());
        QCOMPARE(catalog.applications, QStringList({"Browser", "Music", "Unknown"}));
        QVERIFY(catalog.applicationIcons.isEmpty()); QVERIFY(catalog.error.isEmpty());
    }
    void conflictingGameIdentitiesStayInRoot() {
        QJsonObject window{{"mapped", true}, {"focusHistoryID", 0},
            {"initialClass", "rocketleague.exe"}, {"class", "cs2.exe"}};
        QVERIFY(gameForClients({window}).isEmpty());
        window["class"] = "game.exe";
        QVERIFY(gameForClients({window}).isEmpty());
        window["initialClass"] = "steam_app_123";
        window["initialTitle"] = "Native game";
        QVERIFY(gameForClients({window}).isEmpty());
        window["class"] = "unknown-native-game";
        QCOMPARE(gameForClients({window}), QString("Native game"));
        window["class"] = "cs2.exe";
        QCOMPARE(gameForClients({window}), QString("Counter-Strike 2"));
    }
    void unidentifiedGamePreservesExistingClip() {
        QTemporaryDir directory;
        const auto path = directory.filePath("original.mp4");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("saved clip"); file.close();
        QString error;
        QCOMPARE(routeClip(directory.path(), path, {}, &error), path);
        QVERIFY(error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("saved clip"));
        QCOMPARE(QDir(directory.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
    }
    void plasmaFindsOnlyUnambiguousRunningGames() {
        QTemporaryDir directory;
        const auto root = directory.path();
        QVERIFY(QDir(root).mkdir("101"));
        QFile wine(directory.filePath("wine64")); QVERIFY(wine.open(QIODevice::WriteOnly)); wine.close();
        QVERIFY(QFile::link(wine.fileName(), directory.filePath("101/exe")));
        QFile cmdline(directory.filePath("101/cmdline")); QVERIFY(cmdline.open(QIODevice::WriteOnly));
        const auto command = QByteArrayLiteral("wine64\0C:\\Games\\rocketleague.exe\0");
        cmdline.write(command); cmdline.close();
        QCOMPARE(gameForProcesses(root), QString("Rocket League"));
        QVERIFY(QDir(root).mkdir("102"));
        QFile game(directory.filePath("cs2")); QVERIFY(game.open(QIODevice::WriteOnly)); game.close();
        QVERIFY(QFile::link(game.fileName(), directory.filePath("102/exe")));
        QVERIFY(gameForProcesses(root).isEmpty());
        QVERIFY(QDir(directory.filePath("101")).removeRecursively());
        QCOMPARE(gameForProcesses(root), QString("Counter-Strike 2"));
        QVERIFY(QDir(directory.filePath("102")).removeRecursively());
        QVERIFY(gameForProcesses(root).isEmpty());
    }
    void plasmaGameDetectionDoesNotQueryHyprland() {
        QTemporaryDir directory;
        const auto previousPath = qgetenv("PATH"), previousDesktop = qgetenv("XDG_CURRENT_DESKTOP");
        const auto previousSession = qgetenv("XDG_SESSION_DESKTOP");
        const auto restore = qScopeGuard([&] {
            qputenv("PATH", previousPath); qputenv("XDG_CURRENT_DESKTOP", previousDesktop);
            qputenv("XDG_SESSION_DESKTOP", previousSession);
        });
        QFile fake(directory.filePath("hyprctl")); QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\necho '[{\"mapped\":true,\"focusHistoryID\":0,\"initialClass\":\"rocketleague.exe\"}]'\n");
        fake.close(); QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("PATH", directory.path().toUtf8()); qputenv("XDG_CURRENT_DESKTOP", "KDE");
        qputenv("XDG_SESSION_DESKTOP", "KDE");
        QCOMPARE(currentGame(), gameForProcesses("/proc"));
        qputenv("XDG_CURRENT_DESKTOP", "Hyprland");
        QCOMPARE(currentGame(), QString("Rocket League"));
    }
    void queuedStopKeepsReplayAndRecordingIdentity() {
        QTemporaryDir directory;
        const auto script = directory.filePath("recorder");
        QFile fake(script); QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write(R"PY(#!/usr/bin/env python3
import sys,socket,json,pathlib,time
args=sys.argv
sock=socket.socket(socket.AF_UNIX); sock.bind(args[args.index('-ipc')+1]); sock.listen()
root=pathlib.Path(args[args.index('-o')+1])
while True:
    peer,_=sock.accept()
    data=peer.makefile('rb').readline()
    if not data:
        peer.close(); continue
    msg=json.loads(data); name=msg['name']; result=''
    if name in ['save-replay','stop-replay-recording']:
        time.sleep(.05)
        path=root/(name+'.mp4'); path.write_bytes(b'clip'); result=str(path)
    peer.sendall((json.dumps({'id':msg['id'],'result':'ok','data':result})+'\n').encode()); peer.close()
    if name=='stop': break
)PY");
        fake.close(); QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        GsrOptions options; options.source = "DP-1"; options.systemAudio = false; options.outputDirectory = directory.path();
        GsrCapture engine(nullptr, script);
        QSignalSpy started(&engine, &GsrCapture::started), clips(&engine, &GsrCapture::clipSaved), stopped(&engine, &GsrCapture::stopped);
        engine.start(options, supported); QTRY_COMPARE(started.size(), 1);
        engine.startReplayRecording(); QTRY_VERIFY(engine.replayRecording()); QTRY_VERIFY(!engine.busy());
        engine.saveReplay(2); engine.stop();
        QTRY_COMPARE(clips.size(), 2); QTRY_COMPARE(stopped.size(), 1);
        QCOMPARE(clips[0].size(), 2);
        QCOMPARE(clips[0][1].toBool(), false);
        QCOMPARE(clips[1][1].toBool(), true);
    }
    void removesPreviewSocketBeforeCaptureRestart() {
        QTemporaryDir directory;
        GsrCapture engine(nullptr, "/bin/false");
        const auto stalePath = engine.overlaySocketPath() + ".preview";
        QFile stale(stalePath); QVERIFY(stale.open(QIODevice::WriteOnly)); stale.close();
        QVERIFY(QFile::exists(stalePath));
        GsrOptions options; options.systemAudio = false; options.source = "DP-1"; options.outputDirectory = directory.path();
        QSignalSpy stopped(&engine, &GsrCapture::stopped);
        engine.start(options, supported);
        QTRY_COMPARE(stopped.size(), 1);
        QVERIFY(!QFile::exists(stalePath));
    }
    void waitsForCompositionBeforeReportingReady() {
        QTemporaryDir directory;
        const auto program = directory.filePath("recorder.py");
        QFile script(program); QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(R"(#!/usr/bin/python
import sys,socket,time,json,os,pathlib
server=socket.socket(socket.AF_UNIX);server.bind(sys.argv[sys.argv.index('-ipc')+1]);server.listen(8)
pathlib.Path(__file__+'.ready').touch()
time.sleep(.5)
overlay=socket.socket(socket.AF_UNIX);overlay.bind(os.environ['SHADOWLESS_OVERLAY_SOCKET']);overlay.listen(1)
running=True
while running:
    peer,_=server.accept();data=b''
    while b'\n' not in data:
        part=peer.recv(4096)
        if not part:break
        data+=part
    if data:
        request=json.loads(data);peer.sendall(json.dumps({'id':request['id'],'result':'ok'}).encode()+b'\n')
        if request['name']=='stop':running=False
    peer.close()
)");
        script.close(); QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const auto plugin = directory.filePath("trusted.so"); QFile binary(plugin); QVERIFY(binary.open(QIODevice::WriteOnly)); binary.write("fixture"); binary.close();
        GsrOptions options; options.systemAudio = false; options.source = "DP-1"; options.outputDirectory = directory.path(); options.overlayPlugin = plugin;
        GsrCapture engine(nullptr, program); QSignalSpy started(&engine, &GsrCapture::started), stopped(&engine, &GsrCapture::stopped);
        engine.start(options, supported);
        QTRY_VERIFY(QFile::exists(program + ".ready")); QTest::qWait(200); QCOMPARE(started.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(started.size(), 1, 2000);
        engine.stop(); QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 1, 2000);
    }
    void capabilities() {
        auto info = GsrCapabilities::parse(
            "section=system_info\ndisplay_server|wayland\ngsr_version|6.1.5\n"
            "section=gpu_info\nvendor|nvidia\nsection=video_codecs\nh264\nh264_software\nhevc\nhevc_hdr\nhevc_vulkan\n"
            "section=capture_options\nDP-1|1920x1080\n/dev/video0|1920x1080@30hz|mjpeg\nregion\nportal\n");
        QCOMPARE(info.version, "6.1.5");
        QCOMPARE(info.vendor, "nvidia");
        QCOMPARE(info.displayServer, "wayland");
        QCOMPARE(info.codecs, QStringList({"h264", "hevc"}));
        QCOMPARE(info.sources, QStringList({"DP-1", "portal"}));
    }

    void invalidOptions() {
        GsrOptions options;
        options.outputDirectory = "/tmp/clips";
        QVERIFY(options.validate(supported).isEmpty());
        auto invalid = options;
        invalid.codec = "av1"; QVERIFY(!invalid.validate(supported).isEmpty());
        invalid = options; invalid.source = "HDMI-99"; QVERIFY(!invalid.validate(supported).isEmpty());
        invalid = options; invalid.outputDirectory.clear(); QVERIFY(!invalid.validate(supported).isEmpty());
        for (int fps : {0, 241}) { invalid = options; invalid.frameRate = fps; QVERIFY(!invalid.validate(supported).isEmpty()); }
        for (int seconds : {0, 3601}) { invalid = options; invalid.bufferSeconds = seconds; QVERIFY(!invalid.validate(supported).isEmpty()); }
        for (int rate : {-1, 101}) { invalid = options; invalid.bitrateMbps = rate; QVERIFY(!invalid.validate(supported).isEmpty()); }
        invalid = options; invalid.container = "exe"; QVERIFY(!invalid.validate(supported).isEmpty());
    }

    void captureArguments() {
        GsrOptions options; options.dynamicApplicationAudio=false;
        options.source = "DP-1";
        options.outputDirectory = "/tmp/clips with spaces/$(literal)";
        options.microphone = true;
        options.bitrateMbps = 35;
        auto args = options.arguments("/tmp/engine.sock");
        auto value = [&args](const QString &flag) { return args.value(args.indexOf(flag) + 1); };
        QCOMPARE(value("-o"), options.outputDirectory);
        QCOMPARE(value("-a"), "name:PC + Microphone|default_output|default_input");
        QCOMPARE(value("-fallback-cpu-encoding"), "no");
        QCOMPARE(value("-bm"), "cbr");
        QCOMPARE(value("-q"), "35000");
        QCOMPARE(value("-r"), "30");
        QCOMPARE(value("-restart-replay-on-save"), "no");
        options.separateAudioTracks = true;
        args = options.arguments("/tmp/engine.sock");
        QCOMPARE(args.count("-a"), 2);
        options.continuousRecording = true;
        args = options.arguments("/tmp/engine.sock");
        QVERIFY(!args.contains("-r"));
        QVERIFY(value("-o").startsWith(options.outputDirectory + "/Recording_"));
        QVERIFY(value("-o").endsWith(".mp4"));
        options.overlayPlugin = "/tmp/trusted shadowless plugin.so";
        args = options.arguments("/tmp/engine.sock");
        QCOMPARE(value("-p"), options.overlayPlugin);
        QCOMPARE(value("-cursor"), "no");
    }

    void routesSelectedAudioWithoutRecordingTheDesktopTwice() {
        GsrOptions options; options.dynamicApplicationAudio=false;
        options.outputDirectory = "/tmp/clips";
        options.audioOutput = "studio.monitor";
        options.audioInput = "usb-mic";
        options.microphone = true;
        QCOMPARE(options.audioTracks(), QStringList{"name:PC + Microphone|device:studio.monitor|device:usb-mic"});
        options.separateAudioTracks = true;
        QCOMPARE(options.audioTracks(), QStringList({"name:PC|device:studio.monitor", "name:Microphone|device:usb-mic"}));
        options.applicationAudio = true;
        options.applicationTracks = {{"Browser", 2}, {"Game", 1}, {"Voice Chat", 2}};
        QVERIFY(options.validate(supported).isEmpty());
        QCOMPARE(options.audioTracks(), QStringList({"app:Game|device:usb-mic", "app:Browser|app:Voice Chat"}));
        options.applicationTracks.insert("Other|default_output", 1);
        QVERIFY(!options.validate(supported).isEmpty());
    }
    void fourNamedTracksKeepSourcesSeparate() {
        GsrOptions options; options.fourTrackAudio = true; options.outputDirectory = "/tmp/clips";
        options.applicationAudio = true; options.pcTrack = 1;
        options.applicationTracks = {{"Game", 2}, {"Spotify", 3}, {"Music", 3}};
        options.microphone = true; options.microphoneTrack = 4;
        QCOMPARE(options.audioTracks(), QStringList({
            "name:PC|app-inverse:Game|app-inverse:Music|app-inverse:Spotify",
            "name:Game|app:Game", "name:Music|app:Music|app:Spotify", "name:Microphone|default_input"}));
        QCOMPARE(options.arguments("/tmp/engine.sock").count("-a"), 4);
    }
    void fourTrackSetupRequiresBothExplicitRoles() {
        GsrOptions options; options.outputDirectory = "/tmp/clips";
        options.applicationAudio = true; options.fourTrackAudio = true;
        options.pcTrack = 1; options.microphone = true; options.microphoneTrack = 4;
        options.applicationTracks = {{"Not currently running game", 2}, {"Quiet music player", 3}};
        QVERIFY(options.validate(supported).isEmpty());
        options.applicationTracks.remove("Quiet music player");
        QVERIFY(options.validate(supported).contains("music"));
        options.applicationTracks.insert("Quiet music player", 3);
        options.microphone = false; QVERIFY(!options.validate(supported).isEmpty());
        options.microphone = true; options.pcTrack = 3; QVERIFY(!options.validate(supported).isEmpty());
        options.pcTrack = 1; options.applicationTracks.insert("NOT CURRENTLY RUNNING GAME", 3);
        QVERIFY(!options.validate(supported).isEmpty());
        options.applicationTracks.remove("NOT CURRENTLY RUNNING GAME");
        options.applicationTracks.insert("Other", 5); QVERIFY(!options.validate(supported).isEmpty());
        options.applicationTracks.remove("Other"); options.applicationAudio = false;
        QVERIFY(!options.validate(supported).isEmpty());
    }
    void pcTrackExcludesAllExplicitApplications() {
        GsrOptions options; options.outputDirectory = "/tmp/clips";
        options.applicationAudio = true; options.pcTrack = 3;
        options.applicationTracks = {{"Game", 1}, {"Spotify", 2}, {"YT Music", 2}};
        options.microphone = true; options.microphoneTrack = 4;
        QVERIFY(options.validate(supported).isEmpty());
        QCOMPARE(options.audioTracks(), QStringList({"app:Game", "app:Spotify|app:YT Music",
            "app-inverse:Game|app-inverse:Spotify|app-inverse:YT Music", "default_input"}));
        options.pcTrack = 2; QVERIFY(!options.validate(supported).isEmpty());
        options.pcTrack = 4; QVERIFY(!options.validate(supported).isEmpty());
        options.pcTrack = 7; QVERIFY(!options.validate(supported).isEmpty());
        options.pcTrack = -1; QVERIFY(!options.validate(supported).isEmpty());
        options.pcTrack = 3; options.applicationTracks.clear(); options.microphone = false;
        QVERIFY(options.validate(supported).isEmpty());
        QCOMPARE(options.audioTracks(), QStringList{"app-inverse:"});
    }

    void ipcReplies_data() {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<bool>("success");
        QTest::addColumn<QString>("result");
        QTest::newRow("saved path") << QByteArray("{\"id\":42,\"result\":\"ok\",\"data\":\"/tmp/clip with spaces.mp4\"}\n") << true << QString("/tmp/clip with spaces.mp4");
        QTest::newRow("engine error") << QByteArray("{\"id\":42,\"result\":\"error\",\"data\":\"no recording is running\"}\n") << false << QString("no recording is running");
        QTest::newRow("wrong id") << QByteArray("{\"id\":99,\"result\":\"ok\"}\n") << false << QString();
        QTest::newRow("malformed") << QByteArray("not json\n") << false << QString();
        QTest::newRow("unknown result") << QByteArray("{\"id\":42,\"result\":\"surprise\"}\n") << false << QString();
    }

    void ipcReplies() {
        QFETCH(QByteArray, reply); QFETCH(bool, success); QFETCH(QString, result);
        QTemporaryDir directory;
        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("ipc.sock")));
        QByteArray received;
        connect(&server, &QLocalServer::newConnection, this, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QLocalSocket::readyRead, this, [&, peer] {
                received += peer->readAll();
                if (!received.contains('\n')) return;
                peer->write(reply.left(12));
                QTimer::singleShot(15, peer, [peer, reply] { peer->write(reply.mid(12)); });
            });
        });
        GsrRequest request;
        QSignalSpy finished(&request, &GsrRequest::finished);
        request.send(server.fullServerName(), 42, "save-replay", QJsonObject{{"seconds", 7}}, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1500);
        QCOMPARE(finished[0][0].toBool(), success);
        if (!result.isEmpty()) QCOMPARE(finished[0][1].toString(), result);
        auto message = QJsonDocument::fromJson(received).object();
        QCOMPARE(message["name"].toString(), "save-replay");
        QCOMPARE(message["data"].toObject()["seconds"].toInt(), 7);
    }

    void ipcTimeout() {
        QTemporaryDir directory;
        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("ipc.sock")));
        GsrRequest request;
        QSignalSpy finished(&request, &GsrRequest::finished);
        request.send(server.fullServerName(), 1, "save-replay", {}, 30);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
        QCOMPARE(finished[0][0].toBool(), false);
        QVERIFY(finished[0][1].toString().contains("timed out"));
    }

    void failedLaunch() {
        GsrCapture engine(nullptr, "/no/such/shadowless-gsr-engine");
        GsrOptions options; options.systemAudio = false; options.outputDirectory = QDir::tempPath();
        QSignalSpy failed(&engine, &GsrCapture::error);
        engine.start(options, supported);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 1500);
        QVERIFY(failed[0][0].toString().contains("Cannot start"));
        QVERIFY(!engine.active());
        QVERIFY(!engine.running());
    }

    void liveCapture_data() {
        QTest::addColumn<QString>("codec");
        QTest::newRow("h264") << QString("h264");
        QTest::newRow("hevc") << QString("hevc");
    }

    void livePcTracksDoNotDuplicateSelectedAudio_data() {
        QTest::addColumn<bool>("pcOnly");
        QTest::newRow("four isolated tracks") << false;
        QTest::newRow("PC without assigned applications") << true;
    }
    void livePcTracksDoNotDuplicateSelectedAudio() {
        QFETCH(bool, pcOnly);
        const auto app = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_APP");
        const auto music = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_MUSIC");
        const auto pc = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_PC");
        const auto input = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_INPUT");
        if (app.isEmpty() || music.isEmpty() || pc.isEmpty() || input.isEmpty())
            QSKIP("Requires explicit synthetic game, music, PC and microphone fixtures.");
        const auto pactl = [](const QStringList &args) {
            QProcess p; p.start("pactl", args);
            if (!p.waitForFinished(3000) || p.exitCode()) return QByteArray();
            return p.readAllStandardOutput();
        };
        const auto devices = [&](const QString &kind) {
            QStringList names;
            for (const auto &v : QJsonDocument::fromJson(pactl({"-f", "json", "list", kind})).array())
                names.append(v.toObject().value("name").toString());
            names.sort(); return names;
        };
        const auto sources = devices("sources"), sinks = devices("sinks");
        const auto defaultSource = pactl({"get-default-source"}), defaultSink = pactl({"get-default-sink"});
        QVERIFY(!sources.isEmpty()); QVERIFY(!sinks.isEmpty());
        QTemporaryDir clips; QString error; const auto caps = GsrCapabilities::probe(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        GsrOptions options; options.source = qEnvironmentVariable("SHADOWLESS_GSR_SOURCE", "DP-1");
        options.outputDirectory = clips.path(); options.applicationAudio = true;
        options.applicationTracks = {{app, 1}, {music, 2}}; options.pcTrack = 3;
        if (pcOnly) options.applicationTracks.clear();
        options.microphone = !pcOnly; options.microphoneTrack = 4; options.audioInput = input;
        GsrCapture engine; QSignalSpy started(&engine, &GsrCapture::started), saved(&engine, &GsrCapture::clipSaved);
        QSignalSpy errors(&engine, &GsrCapture::error), stopped(&engine, &GsrCapture::stopped);
        engine.start(options, caps);
        QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty() || !errors.isEmpty(), 10000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        QCOMPARE(devices("sources"), sources); QCOMPARE(devices("sinks"), sinks);
        QCOMPARE(pactl({"get-default-source"}), defaultSource); QCOMPARE(pactl({"get-default-sink"}), defaultSink);
        QProcess lateMusic;
        const auto cleanup = qScopeGuard([&] {
            if (lateMusic.state() != QProcess::NotRunning) { lateMusic.terminate(); lateMusic.waitForFinished(2000); }
        });
        const auto lateFile = qEnvironmentVariable("SHADOWLESS_QA_LATE_MUSIC_FILE");
        if (!lateFile.isEmpty()) {
            const auto output = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_OUTPUT");
            QVERIFY(output.endsWith(".monitor"));
            const auto props = QString::fromUtf8(QJsonDocument(QJsonObject{{"application.name", music}}).toJson(QJsonDocument::Compact));
            lateMusic.start("pw-play", {"--target", output.chopped(8), "--properties", props, lateFile});
            QVERIFY(lateMusic.waitForStarted(2000));
        }
        QTest::qWait(2800); engine.saveReplay(2);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 10000);
        engine.stop(); QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 1, 10000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        const int frequencies[]{641, 997, 1237, 883};
        for (int track = 0; track < (pcOnly ? 1 : 4); ++track) {
            for (int tone = 0; tone < 4; ++tone) {
                QProcess ffmpeg;
                ffmpeg.start("ffmpeg", {"-hide_banner", "-i", saved[0][0].toString(), "-map", QString("0:a:%1").arg(track),
                    "-af", QString("bandpass=f=%1:width_type=h:w=12,bandpass=f=%1:width_type=h:w=12,volumedetect").arg(frequencies[tone]), "-f", "null", "-"});
                QVERIFY(ffmpeg.waitForFinished(5000)); QCOMPARE(ffmpeg.exitCode(), 0);
                const auto match = QRegularExpression("mean_volume: (-?[0-9.]+) dB").match(QString::fromUtf8(ffmpeg.readAllStandardError()));
                QVERIFY(match.hasMatch()); const auto db = match.captured(1).toDouble();
                qInfo() << "Track" << track << "tone" << frequencies[tone] << "dB" << db;
                const bool expected = pcOnly ? tone < 3 : track == tone;
                QVERIFY(expected ? db > -40 : db < -55);
            }
        }
        QTRY_COMPARE(devices("sources"), sources); QCOMPARE(devices("sinks"), sinks);
    }

    void liveAudioLevels_data() {
        QTest::addColumn<bool>("applications");
        QTest::addColumn<bool>("sharedTrack");
        QTest::newRow("desktop devices") << false << false;
        QTest::newRow("selected app and mic") << true << false;
        QTest::newRow("selected app mixed with mic") << true << true;
    }

    void liveAudioLevels() {
        const auto output = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_OUTPUT");
        const auto input = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_INPUT");
        const auto app = qEnvironmentVariable("SHADOWLESS_QA_AUDIO_APP");
        if (output.isEmpty() || input.isEmpty() || app.isEmpty())
            QSKIP("Requires explicit synthetic PipeWire output, input and application fixtures.");
        QFETCH(bool, applications);
        QFETCH(bool, sharedTrack);
        QTemporaryDir clips;
        QString queryError;
        const auto caps = GsrCapabilities::probe(&queryError);
        QVERIFY2(queryError.isEmpty(), qPrintable(queryError));
        GsrOptions options;
        options.source = qEnvironmentVariable("SHADOWLESS_GSR_SOURCE", "DP-1");
        options.outputDirectory = clips.path();
        options.audioOutput = output;
        options.audioInput = input;
        options.microphone = true;
        options.separateAudioTracks = true;
        options.applicationAudio = applications;
        options.applicationTracks.insert(app, 1);
        options.microphoneTrack = sharedTrack ? 1 : 2;
        GsrCapture engine;
        QSignalSpy started(&engine, &GsrCapture::started), saved(&engine, &GsrCapture::clipSaved);
        QSignalSpy errors(&engine, &GsrCapture::error), stopped(&engine, &GsrCapture::stopped);
        engine.start(options, caps);
        QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty() || !errors.isEmpty(), 10000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        QTest::qWait(2800);
        engine.saveReplay(2);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 10000);
        options.levels.systemVolume = 50;
        options.levels.microphoneMuted = true;
        engine.setAudioLevels(options.levels);
        QTest::qWait(4400);
        engine.saveReplay(2);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 2, 10000);
        engine.stop();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 1, 10000);
        QCOMPARE(started.size(), 1);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));

        const auto meanVolume = [](const QString &path, int track, int frequency = 0) {
            const QString filter = frequency == 0 ? "volumedetect" :
                QString("bandpass=f=%1:width_type=h:w=12,volumedetect").arg(frequency);
            QProcess process;
            process.start("ffmpeg", {"-hide_banner", "-i", path, "-map", QString("0:a:%1").arg(track),
                                     "-af", filter, "-f", "null", "-"});
            if (!process.waitForFinished(5000) || process.exitCode() != 0) return 0.0;
            const auto match = QRegularExpression("mean_volume: (-?[0-9.]+) dB").match(
                QString::fromUtf8(process.readAllStandardError()));
            return match.captured(1).toDouble();
        };
        const double firstOutput = meanVolume(saved[0][0].toString(), 0, sharedTrack ? 641 : 0);
        const double firstMic = meanVolume(saved[0][0].toString(), sharedTrack ? 0 : 1, sharedTrack ? 883 : 0);
        const double lastOutput = meanVolume(saved[1][0].toString(), 0, sharedTrack ? 641 : 0);
        const double lastMic = meanVolume(saved[1][0].toString(), sharedTrack ? 0 : 1, sharedTrack ? 883 : 0);
        qInfo() << "Track means before and after:" << firstOutput << firstMic << lastOutput << lastMic;
        QVERIFY(firstOutput < -10 && firstOutput > -40);
        QVERIFY(firstMic < -10 && firstMic > -40);
        QVERIFY(qAbs((firstOutput - lastOutput) - (applications ? 0.0 : 6.0)) < 2.0);
        QVERIFY(lastMic < (sharedTrack ? -48.0 : -80.0));
    }

    void liveCapture() {
        if (qEnvironmentVariable("SHADOWLESS_GSR_LIVE") != "1")
            QSKIP("Set SHADOWLESS_GSR_LIVE=1 to record the selected monitor and system audio.");
        QFETCH(QString, codec);
        QString queryError;
        auto capabilities = GsrCapabilities::probe(&queryError);
        QVERIFY2(queryError.isEmpty(), qPrintable(queryError));
        if (!capabilities.codecs.contains(codec)) QSKIP("Hardware codec is unavailable on this GPU.");
        const auto root = qEnvironmentVariable("SHADOWLESS_QA_OUTPUT", QDir::currentPath() + "/live-clips");
        const auto output = QDir(root).filePath(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz-") + codec);
        QVERIFY(QDir().mkpath(output));
        QFile log(QDir(output).filePath("capture.log"));
        QVERIFY(log.open(QIODevice::WriteOnly));
        GsrCapture engine;
        connect(&engine, &GsrCapture::diagnostic, &log, [&log](const QString &message) { log.write(message.toUtf8() + '\n'); log.flush(); });
        QSignalSpy started(&engine, &GsrCapture::started), stopped(&engine, &GsrCapture::stopped);
        QSignalSpy clips(&engine, &GsrCapture::clipSaved), errors(&engine, &GsrCapture::error);
        QSignalSpy recording(&engine, &GsrCapture::recordingChanged);
        GsrOptions options;
        options.source = qEnvironmentVariable("SHADOWLESS_GSR_SOURCE", "DP-1");
        options.outputDirectory = output;
        options.codec = codec;
        options.bufferSeconds = 10;
        engine.start(options, capabilities);
        QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty() || !errors.isEmpty(), 15000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        QVERIFY(engine.running());
        QTest::qWait(3500);
        engine.saveReplay(2);
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 1, 10000);
        engine.startReplayRecording();
        QTRY_VERIFY_WITH_TIMEOUT(!recording.isEmpty() && recording.last()[0].toBool(), 5000);
        QTest::qWait(3000);
        engine.stopReplayRecording();
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 2, 10000);
        engine.startReplayRecording();
        QTRY_VERIFY_WITH_TIMEOUT(recording.last()[0].toBool(), 5000);
        QTest::qWait(2000);
        engine.saveReplay(2);
        // Stop must finish the snapshot and the simultaneous recording.
        engine.stop();
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 4, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 1, 10000);
        QVERIFY(!engine.active());
        QVERIFY(!engine.running());

        options.continuousRecording = true;
        engine.start(options, capabilities);
        QTRY_COMPARE_WITH_TIMEOUT(started.size(), 2, 15000);
        QTest::qWait(3000);
        engine.stop();
        QTRY_COMPARE_WITH_TIMEOUT(clips.size(), 5, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 2, 10000);
        QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
        for (const auto &saved : clips) {
            const auto path = saved[0].toString();
            QProcess probe;
            probe.start("ffprobe", {"-v", "error", "-show_entries", "stream=codec_name,width,height,avg_frame_rate:format=duration", "-of", "json", path});
            QVERIFY(probe.waitForFinished(10000));
            QCOMPARE(probe.exitCode(), 0);
            const auto metadata = probe.readAllStandardOutput();
            QFile report(path + ".json"); QVERIFY(report.open(QIODevice::WriteOnly)); report.write(metadata);
            const auto info = QJsonDocument::fromJson(metadata).object();
            const auto streams = info["streams"].toArray();
            QVERIFY(streams.size() >= 2);
            QCOMPARE(streams[0].toObject()["codec_name"].toString(), codec);
            QVERIFY(streams[0].toObject()["width"].toInt() > 0);
            QVERIFY(streams[0].toObject()["height"].toInt() > 0);
            QCOMPARE(streams[1].toObject()["codec_name"].toString(), "aac");
            QVERIFY(info["format"].toObject()["duration"].toString().toDouble() > 1.0);
            QProcess decode;
            decode.start("ffmpeg", {"-v", "error", "-i", path, "-f", "null", "-"});
            QVERIFY(decode.waitForFinished(30000));
            QCOMPARE(decode.exitCode(), 0);
            QVERIFY2(decode.readAllStandardError().isEmpty(), qPrintable(path));
        }
        qInfo().noquote() << "Validated clips:" << output;
    }
};

QTEST_GUILESS_MAIN(GsrTests)
#include "tst_gsr.moc"
