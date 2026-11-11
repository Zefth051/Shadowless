#include "../ui/dashboard.h"
#include "../ui/library.h"
#include "../ui/settings_page.h"
#include "../ui/overlay_editor.h"
#include "../ui/status_indicator.h"
#include "../ui/clip_player.h"
#include "../player/mpv_player.h"
#include "../player/media_tools.h"
#include "../app/controller.h"
#include "../capture/replay_paths.h"
#include "../ui/clip_notification.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSpinBox>
#include <QScrollArea>
#include <QWheelEvent>
#include <algorithm>
#include <clocale>
#include <QScopeGuard>

using namespace Shadowless;
Q_DECLARE_METATYPE(Shadowless::GsrOptions)
Q_DECLARE_METATYPE(Shadowless::OverlayScene)

class UiTests : public QObject {
    Q_OBJECT
    const GsrCapabilities caps{"6.1.5", "nvidia", "wayland", {"h264", "hevc"}, {"DP-1"}};
    const QStringList keys{"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"};
    QPushButton *button(QWidget *parent, const QString &text) {
        for (auto *candidate : parent->findChildren<QPushButton *>()) if (candidate->text() == text) return candidate;
        return nullptr;
    }
private slots:
    void libraryKeepsNewestTwentyInEveryCollection() {
        QTemporaryDir directory;
        const auto origin = QDateTime::fromSecsSinceEpoch(1700000000);
        for (const auto &collection : {QString("Older game"), QString("Newer game")}) {
            QVERIFY(QDir(directory.path()).mkpath(collection));
            for (int i = 0; i < 30; ++i) {
                QFile file(directory.filePath(collection + QString("/%1.mp4").arg(i)));
                QVERIFY(file.open(QIODevice::WriteOnly));
                file.write("clip");
                QVERIFY(file.flush());
                QVERIFY(file.setFileTime(origin.addSecs(i + (collection == "Newer game" ? 100 : 0)), QFileDevice::FileModificationTime));
            }
        }
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty());
        QStringList newest;
        for (int i = 29; i >= 10; --i) newest.append(directory.filePath(QString("Newer game/%1.mp4").arg(i)));
        QTRY_COMPARE(clips.last().at(0).toStringList(), newest);
        auto *filter = library.findChild<QComboBox *>("collectionFilter"); QVERIFY(filter);
        filter->setCurrentIndex(filter->findText("Older game"));
        QStringList older;
        for (int i = 29; i >= 10; --i) older.append(directory.filePath(QString("Older game/%1.mp4").arg(i)));
        QCOMPARE(clips.last().at(0).toStringList(), older);
    }
    void libraryFindsAndFiltersGameCollections() {
        QTemporaryDir directory;
        QVERIFY(QDir(directory.path()).mkpath("Rocket League"));
        for (const auto &name : {QString("desktop.mp4"), QString("Rocket League/goal.mp4")}) {
            QFile file(directory.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip");
        }
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty());
        QTRY_COMPARE(clips.last().at(0).toStringList().size(), 2);
        auto *filter = library.findChild<QComboBox *>("collectionFilter"); QVERIFY(filter);
        const int game = filter->findText("Rocket League"); QVERIFY(game >= 0);
        filter->setCurrentIndex(game);
        QTRY_COMPARE(clips.last().at(0).toStringList(), QStringList{directory.filePath("Rocket League/goal.mp4")});
        for (int i = 0; i < 25; ++i) {
            QFile file(directory.filePath(QString("desktop-%1.mp4").arg(i)));
            QVERIFY(file.open(QIODevice::WriteOnly)); file.write("newer clip");
        }
        QSignalSpy scanned(&library, &Library::storageChanged);
        library.refresh(directory.path()); QTRY_VERIFY(!scanned.isEmpty());
        QCOMPARE(filter->currentText(), QString("Rocket League"));
        QCOMPARE(clips.last().at(0).toStringList(), QStringList{directory.filePath("Rocket League/goal.mp4")});
    }
    void libraryDeletionRequiresConfirmationAndPreservesOtherClips() {
        QTemporaryDir directory;
        const auto path = directory.filePath("delete-me.mp4");
        const auto keep = directory.filePath("keep.mp4");
        for (const auto &name : {path, keep}) {
            QFile file(name); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip");
        }
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        QSignalSpy opened(&library, &Library::clipRequested);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty() && clips.last().at(0).toStringList().size() == 2);
        QPushButton *remove = nullptr;
        for (auto *candidate : library.findChildren<QPushButton *>("deleteClip"))
            if (candidate->accessibleName().contains("delete-me.mp4")) remove = candidate;
        QVERIFY(remove);
        remove->click();
        QVERIFY(QFileInfo::exists(path));
        auto *cancel = library.findChild<QPushButton *>("cancelDeleteClip"); QVERIFY(cancel);
        cancel->click(); QVERIFY(QFileInfo::exists(path));
        remove->click();
        auto *confirm = library.findChild<QPushButton *>("confirmDeleteClip"); QVERIFY(confirm);
        confirm->click();
        QTRY_VERIFY(!QFileInfo::exists(path));
        QTRY_COMPARE(clips.last().at(0).toStringList(), QStringList{keep});
        QVERIFY(QFileInfo::exists(keep));
        QVERIFY(opened.isEmpty());
    }
    void libraryDeletionReportsFailure() {
        QTemporaryDir directory;
        const auto path = directory.filePath("missing.mp4");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("clip"); file.close();
        Library library; library.show();
        QSignalSpy clips(&library, &Library::clipsChanged);
        library.refresh(directory.path());
        QTRY_VERIFY(!clips.isEmpty() && clips.last().at(0).toStringList().size() == 1);
        auto *remove = library.findChild<QPushButton *>("deleteClip"); QVERIFY(remove); remove->click();
        QVERIFY(QFile::remove(path));
        library.findChild<QPushButton *>("confirmDeleteClip")->click();
        auto *error = library.findChild<QLabel *>("deleteClipError"); QVERIFY(error);
        QVERIFY(error->isVisible()); QVERIFY(!error->text().isEmpty());
    }
    void settingsOffersOptionalGameOrganization() {
        SettingsPage page(caps); GsrOptions options; options.source = "DP-1"; options.outputDirectory = "/tmp/clips";
        page.edit(options, false, false, keys);
        auto *check = page.findChild<QCheckBox *>("organizeByGame"); QVERIFY(check);
        QVERIFY(!check->isChecked());
        check->setChecked(true);
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1);
        const auto changed = qvariant_cast<GsrOptions>(saved.at(0).at(0));
        QVERIFY(changed.organizeByGame);
        QTemporaryDir directory; QString error;
        Settings settings(directory.filePath("settings.ini"));
        QVERIFY(settings.save(changed, {}, false, &error));
        QVERIFY(settings.capture(caps).organizeByGame);
    }
    void clipRoutingPreservesFilesAndRejectsSymlinkFolders() {
        QTemporaryDir directory, outside; QString error;
        const auto path = directory.filePath("goal.mp4");
        auto create = [&] { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("new clip"); };
        create();
        QCOMPARE(routeClip(directory.path(), path, {}, &error), path);
        const auto first = routeClip(directory.path(), path, "Rocket League", &error);
        QVERIFY(error.isEmpty()); QCOMPARE(first, directory.filePath("Rocket League/goal.mp4"));
        create();
        const auto second = routeClip(directory.path(), path, "Rocket League", &error);
        QVERIFY(error.isEmpty()); QVERIFY(first != second); QVERIFY(QFileInfo::exists(first)); QVERIFY(QFileInfo::exists(second));
        create();
        QVERIFY(QFile::link(outside.path(), directory.filePath("Linked")));
        QCOMPARE(routeClip(directory.path(), path, "Linked", &error), path);
        QVERIFY(!error.isEmpty()); QVERIFY(QFileInfo::exists(path));
        const auto sanitized = routeClip(directory.path(), path, "../../escape", &error);
        QVERIFY(error.isEmpty()); QCOMPARE(QFileInfo(sanitized).absolutePath(), directory.filePath(".._.._escape"));
    }
    void gameIdentificationRejectsUnknownForegroundAndAmbiguousExecutables() {
        QJsonObject game{{"mapped", true}, {"initialClass", "rocketleague.exe"}, {"focusHistoryID", 0}};
        QCOMPARE(gameForClients({game}), QString("Rocket League"));
        game["initialClass"] = "game.exe";
        QVERIFY(gameForClients({game}).isEmpty());
        game["initialClass"] = "rocketleague.exe"; game["focusHistoryID"] = 1;
        QJsonObject browser{{"mapped", true}, {"initialClass", "firefox"}, {"focusHistoryID", 0}};
        QVERIFY(gameForClients({game, browser}).isEmpty());
        game["hidden"] = true; QVERIFY(gameForClients({game}).isEmpty());
        game["hidden"] = false; game["focusHistoryID"] = -1;
        QVERIFY(gameForClients({game}).isEmpty());
    }
    void clipNotificationReplacesItsTimeout() {
        ClipNotification toast;
        toast.present("Saving", {}, 20);
        toast.present("Saved", "goal.mp4", 120);
        QTest::qWait(45); QVERIFY(toast.isVisible());
        QTRY_VERIFY(!toast.isVisible());
        toast.present("Saved", "goal.mp4"); toast.dismiss(); QVERIFY(!toast.isVisible());
    }
    void clipNotificationPaintsCardAndStatusIcon() {
        ClipNotification toast; toast.present("Clip saved", "goal.mp4");
        const auto image = toast.grab().toImage();
        const auto card = image.pixelColor(180, 75);
        QVERIFY(card.alpha() > 200 && card.red() < 50 && card.green() < 50);
        int accentPixels = 0;
        for (int y = 20; y < 65; ++y) for (int x = 12; x < 60; ++x) {
            const auto pixel = image.pixelColor(x, y);
            if (pixel.green() > 130 && pixel.green() > pixel.red() * 1.3) ++accentPixels;
        }
        QVERIFY(accentPixels > 50);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(image.save(output + ".notification.png"));
    }
    void clipNotificationCountsDownAndRestarts() {
        ClipNotification toast;
        toast.setAccent(QColor("#a855f7"));
        const auto barPixels = [&toast] {
            const auto image = toast.grab().toImage();
            int count = 0;
            for (int x = 15; x < toast.width() - 15; ++x) {
                const auto pixel = image.pixelColor(x, toast.height() - 2);
                if (pixel.red() > 100 && pixel.blue() > 150) ++count;
            }
            return count;
        };
        toast.present("Clip saved", "first.mp4", 1000);
        const int full = barPixels();
        QVERIFY(full > 300);
        QTest::qWait(400);
        const int remaining = barPixels();
        QVERIFY(remaining < full * .8);
        QVERIFY(remaining > 0);
        toast.present("Clip saved", "second.mp4", 1000);
        QVERIFY(barPixels() > remaining + 50);
        toast.dismiss();
        QVERIFY(!toast.isVisible());
        for (auto *animation : toast.findChildren<QVariantAnimation *>())
            QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    }
    void controllerCreatesPassiveClipNotification() {
        Controller controller(caps, {}, true);
        QWidget *toast = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) if (widget->objectName() == "clipNotification") toast = widget;
        QVERIFY(toast);
        QVERIFY(toast->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
        QVERIFY(toast->windowFlags().testFlag(Qt::WindowTransparentForInput));
        QVERIFY(toast->testAttribute(Qt::WA_ShowWithoutActivating));
        QVERIFY(toast->testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(!toast->isVisible());
        QFile sound(":/shadowless/replay-saved.wav"); QVERIFY(sound.open(QIODevice::ReadOnly));
        QCOMPARE(sound.read(4), QByteArray("RIFF"));
    }
    void dashboardModeCardsFitTheirContainer() {
        Dashboard dashboard(caps);
        dashboard.show();
        const auto tiles = dashboard.findChildren<QPushButton *>("captureMode");
        QCOMPARE(tiles.size(), 2);
        for (const auto size : {QSize(1280, 720), QSize(1920, 1080), QSize(740, 720)}) {
            dashboard.resize(size);
            QCoreApplication::processEvents();
            for (auto *tile : tiles) {
                QVERIFY(tile->height() >= 140);
                QVERIFY(tile->parentWidget()->rect().contains(tile->geometry()));
            }
        }
    }
    void initTestCase() { qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/dev/null"); }
    void offersFourIndicatorCorners() {
        SettingsPage page(caps);
        auto *position = page.findChild<QComboBox *>("indicatorPosition");
        QVERIFY(position);
        QCOMPARE(position->count(), 4);
        QCOMPARE(position->currentData().toString(), "top-right");
        QTemporaryDir directory;
        const auto settingsPath = directory.filePath("settings.ini");
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = directory.path();
        page.edit(options, false, false, keys);
        for (int i = 0; i < 4; ++i) {
            position->setCurrentIndex(i);
            QSignalSpy saved(&page, &SettingsPage::saved);
            button(&page, "Save settings")->click();
            QCOMPARE(saved.size(), 1);
            const auto corner = saved.first().at(4).toString();
            QCOMPARE(corner, position->currentData().toString());
            Settings settings(settingsPath);
            QString error;
            QVERIFY(settings.save(GsrOptions{}, {}, true, &error, keys, "mint", corner));
            QCOMPARE(Settings(settingsPath).indicatorPosition(), corner);
            page.edit(options, true, false, keys, "mint", corner);
            QCOMPARE(position->currentData().toString(), corner);
        }
    }
    void previewsCustomOverlaysOnDashboard() {
        Dashboard dashboard(caps);
        auto *preview = dashboard.findChild<OverlayCanvas *>("overlayPreview");
        QVERIFY(preview);
        QTemporaryDir directory;
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath("custom.png");
        QVERIFY(image.save(path));
        OverlayLayer layer;
        layer.path = path;
        OverlayScene scene;
        scene.enabled = true;
        scene.layers.append(layer);
        dashboard.updateState(GsrOptions{}, scene, false, false, false, false);
        dashboard.show();
        QCoreApplication::processEvents();
        QVERIFY(preview->isVisible());
        const auto rendered = preview->grab().toImage();
        int redPixels = 0;
        for (int y = 0; y < rendered.height(); ++y)
            for (int x = 0; x < rendered.width(); ++x) {
                const auto color = rendered.pixelColor(x, y);
                if (color.red() > 200 && color.green() < 40) ++redPixels;
            }
        QVERIFY(redPixels > 100);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output + ".preview.png"));
        dashboard.updateState(GsrOptions{}, {}, false, false, false, false);
        QVERIFY(!preview->isVisible());
    }
    void includesAnEmbeddedPlayer() {
        Dashboard dashboard(caps);
        auto *player = dashboard.findChild<ClipPlayer *>("clipPlayer");
        QVERIFY(player);
        QTemporaryDir directory;
        const auto path = directory.filePath("sample.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-t", "3", "-c:v", "mpeg4", path}), 0);
        dashboard.show();
        QSignalSpy hidden(&dashboard, &Dashboard::hideRequested);
        auto *library = dashboard.findChild<Library *>();
        library->clipRequested(path);
        QVERIFY(player->isVisible());
        QCOMPARE(hidden.size(), 0);
        auto *media = player->findChild<MpvPlayer *>();
        QVERIFY(media);
        QSignalSpy frames(media, &MpvPlayer::frameRendered);
        QSignalSpy errors(media, &MpvPlayer::error);
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() > 0, 5000);
        QTRY_VERIFY(media->duration() > 0);
        player->findChild<QPushButton *>("clipPlayPause")->click();
        QTRY_VERIFY(media->paused());
        auto *seek = player->findChild<QSlider *>("clipSeek");
        QVERIFY(seek);
        seek->setValue(0);
        seek->setValue(1000);
        QTRY_VERIFY(media->position() >= 900);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output + ".player.png"));
        dashboard.escape();
        QVERIFY(media->paused());
        QVERIFY(media->source().isEmpty());
        QCOMPARE(hidden.size(), 0);
        QCOMPARE(errors.size(), 0);
    }
    void playerOffersEditingAndFullscreen() {
        ClipPlayer player;
        QVERIFY2(player.findChild<QPushButton *>("clipFullscreen"), "Fullscreen transport is missing");
        QVERIFY(player.findChild<QPushButton *>("clipNext"));
        QVERIFY(player.findChild<QPushButton *>("clipPrevious"));
        QVERIFY(player.findChild<QComboBox *>("clipSpeed"));
        QVERIFY(player.findChild<QPushButton *>("clipTrim"));
    }
    void fullscreenControlsKeepVideoGeometry() {
        QTemporaryDir directory; const auto path=directory.filePath("fullscreen.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","12","-c:v","mpeg4",path}),0);
        Dashboard dashboard(caps); dashboard.resize(1600,1000); dashboard.show();
        auto *player=dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media=player->findChild<MpvPlayer *>();
        QTRY_VERIFY(media->loaded());
        player->setFullscreen(true);
        auto *play=player->findChild<QPushButton *>("clipPlayPause");
        QTest::mouseMove(media,QPoint(50,50));
        QCoreApplication::processEvents();
        const auto videoGeometry=media->geometry();
        const auto panelGeometry=dashboard.findChild<QWidget *>("shadowlessPanel")->geometry();
        QCOMPARE(media->mapTo(player,QPoint(0,0)),QPoint(0,0));
        QCOMPARE(media->size(),player->size());
        const auto playCenter=play->mapTo(player,play->rect().center());
        QVERIFY(media->rect().contains(media->mapFrom(player,playCenter)));
        QCOMPARE(player->childAt(playCenter),play);
        QTRY_VERIFY_WITH_TIMEOUT(!play->isVisible(),5000);
        QCoreApplication::processEvents();
        QCOMPARE(media->geometry(),videoGeometry);
        QTest::mouseMove(media,QPoint(80,80));
        QTRY_VERIFY(play->isVisible());
        QCOMPARE(media->geometry(),videoGeometry);
        QCOMPARE(dashboard.findChild<QWidget *>("shadowlessPanel")->geometry(),panelGeometry);
        dashboard.resize(1280,800); QCoreApplication::processEvents();
        QCOMPARE(media->size(),player->size());
        const auto resizedCenter=play->mapTo(player,play->rect().center());
        QCOMPARE(player->childAt(resizedCenter),play);
        player->setFullscreen(false); QCoreApplication::processEvents();
        QVERIFY(play->mapTo(player,QPoint(0,0)).y()>=media->mapTo(player,media->rect().bottomLeft()).y());
    }
    void spacePausesInsteadOfActivatingFocusedTransport() {
        QTemporaryDir directory; const auto path=directory.filePath("space.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","12","-c:v","mpeg4",path}),0);
        ClipPlayer player; player.resize(1000,750); player.show(); player.open(path);
        auto *media=player.findChild<MpvPlayer *>();
        auto *edit=player.findChild<QPushButton *>("clipTrim");
        QTRY_VERIFY(media->loaded()); QTRY_VERIFY(!media->paused());
        for(const auto &name:{"clipTrim","clipFullscreen","clipSeekForward","clipPlayPause"}) {
            auto *button=player.findChild<QPushButton *>(name); button->setFocus();
            QTest::keyClick(button,Qt::Key_Space); QTRY_VERIFY(media->paused());
            QVERIFY(!player.editing()); QVERIFY(!player.fullscreen());
            QTest::keyClick(button,Qt::Key_Space); QTRY_VERIFY(!media->paused());
            QVERIFY(!edit->isChecked());
        }
        edit->click(); QTRY_VERIFY(player.editing());
        QTRY_VERIFY(!player.findChild<MediaTools *>()->busy());
        edit->setFocus();
        const bool paused=media->paused();
        QTest::keyClick(edit,Qt::Key_Space); QTRY_COMPARE(media->paused(),!paused);
        QVERIFY(player.editing());
    }
    void editorExpandsAndResetsAfterDashboard() {
        QTemporaryDir directory; const auto path=directory.filePath("edit.mp4"), other=directory.filePath("other.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","4","-c:v","mpeg4",path}),0);
        QVERIFY(QFile::copy(path,other));
        Dashboard dashboard(caps); dashboard.resize(1600,1000); dashboard.show();
        auto *library=dashboard.findChild<Library *>(); auto *player=dashboard.findChild<ClipPlayer *>();
        auto *edit=player->findChild<QPushButton *>("clipTrim");
        auto *timeline=player->findChild<QWidget *>("clipTimeline");
        auto *panel=dashboard.findChild<QWidget *>("shadowlessPanel");
        for(const auto &clip:{path,path,other}) {
            library->clipRequested(clip);
            QVERIFY(!edit->isChecked()); QVERIFY(!timeline->isVisible());
            const auto normal=panel->geometry();
            edit->click();
            QTRY_VERIFY(panel->width()>dashboard.width()*0.9);
            QVERIFY(panel->height()>dashboard.height()*0.9);
            QVERIFY(panel->width()<dashboard.width()); QVERIFY(panel->height()<dashboard.height());
            player->setFullscreen(true); dashboard.escape();
            QVERIFY(panel->width()>dashboard.width()*0.9);
            edit->click(); QCOMPARE(panel->geometry(),normal);
            edit->click(); dashboard.home();
        }
    }
    void playerSeeksWhereClickedAndDragged() {
        QTemporaryDir directory;
        const auto path = directory.filePath("seek.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-t", "20", "-c:v", "mpeg4", path}), 0);
        ClipPlayer player; player.resize(1000, 650); player.show(); player.open(path);
        auto *media = player.findChild<MpvPlayer *>();
        auto *seek = player.findChild<QSlider *>("clipSeek");
        QTRY_VERIFY(media->duration() >= 19000);
        media->pause(true); QTRY_VERIFY(media->paused());
        QTest::mouseClick(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width()*3/4, seek->height()/2));
        QTRY_VERIFY2(qAbs(media->position()-15000) < 500, qPrintable(QString::number(media->position())));
        QTest::mousePress(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width()/4, seek->height()/2));
        QTest::mouseMove(seek, QPoint(seek->width()/2, seek->height()/2));
        QTest::mouseRelease(seek, Qt::LeftButton, Qt::NoModifier, QPoint(seek->width()/2, seek->height()/2));
        QTRY_VERIFY(qAbs(media->position()-10000) < 500);
        QTest::qWait(200); QVERIFY(qAbs(media->position()-10000) < 500); QVERIFY(media->paused());
        auto *mute=player.findChild<QCheckBox *>("clipMute");
        mute->setFocus(); QTest::keyClick(mute,Qt::Key_Space);
        QVERIFY(mute->isChecked()); QTest::qWait(100); QVERIFY(media->paused());
        media->pause(false); QTRY_VERIFY(!media->paused());
        QTest::mousePress(seek,Qt::LeftButton,Qt::NoModifier,QPoint(seek->width()/4,seek->height()/2));
        QTRY_VERIFY(media->paused());
        QTest::mouseRelease(seek,Qt::LeftButton,Qt::NoModifier,QPoint(seek->width()/4,seek->height()/2));
        QTRY_VERIFY(!media->paused()); QTRY_VERIFY(qAbs(media->position()-5000)<500);
    }
    void playerDragsVisibleTrimHandles() {
        QTemporaryDir directory; const auto path=directory.filePath("trim.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","8","-c:v","mpeg4",path}),0);
        ClipPlayer player; player.resize(1000,750); player.show(); player.open(path);
        auto *media=player.findChild<MpvPlayer *>();
        auto *tools=player.findChild<MediaTools *>();
        QTRY_VERIFY(media->loaded());
        player.findChild<QPushButton *>("clipTrim")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(),15000);
        auto *timeline=player.findChild<QWidget *>("clipTimeline"); QVERIFY(timeline);
        auto *in=player.findChild<QDoubleSpinBox *>("clipIn");
        auto *out=player.findChild<QDoubleSpinBox *>("clipOut");
        const int y=timeline->height()/2, w=timeline->width();
        const auto drag=[&](int from,int to) {
            QTest::mouseMove(timeline,QPoint(from,y));
            QCOMPARE(timeline->cursor().shape(),Qt::SizeHorCursor);
            QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(from,y));
            QTest::mouseMove(timeline,QPoint(to,y));
            QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(to,y));
        };
        // Grab inside each visible handle, including the initial clip edges.
        drag(12,w/4+12);
        QVERIFY(qAbs(in->value()-2)<.05); QCOMPARE(out->value(),8.);
        QTRY_VERIFY(media->paused()); QTRY_VERIFY(qAbs(media->position()-2000)<150);
        drag(w-12,w*3/4-12);
        QVERIFY(qAbs(out->value()-6)<.05);
        drag(qRound(in->value()/8*w)+8,w);
        QVERIFY(in->value()<out->value());
        in->setValue(2);
        drag(qRound(out->value()/8*w)-8,0);
        QVERIFY(out->value()>in->value());
        in->setValue(0); out->setValue(8);
        drag(12,12); QCOMPARE(in->value(),0.);
        drag(w-12,w-12); QCOMPARE(out->value(),8.);
        QTest::mouseMove(timeline,QPoint(w/2,y));
        QCOMPARE(timeline->cursor().shape(),Qt::PointingHandCursor);
        // Both handles must remain reachable when a short range makes them overlap.
        in->setValue(2); out->setValue(2.+16.*8/w);
        const double before=out->value();
        const int end=qRound(before/8*w)-2;
        drag(end,end+w/4);
        QVERIFY(qAbs(out->value()-(before+2))<.05); QCOMPARE(in->value(),2.);
        in->setValue(1); out->setValue(6);
        QTest::mouseClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(1,y));
        QTRY_VERIFY(qAbs(media->position()-1000)<150);
        QTest::keyClick(media,Qt::Key_Right);
        QTest::keyClick(media,Qt::Key_Right);
        QTRY_VERIFY(qAbs(media->position()-6000)<150);
        player.findChild<QPushButton *>("clipPlayPause")->click();
        QTRY_VERIFY(!media->paused());
        QTRY_VERIFY(media->position()>=1000 && media->position()<3000);
        out->setValue(2);
        QTRY_VERIFY_WITH_TIMEOUT(media->paused(),3000);
        QVERIFY(qAbs(media->position()-2000)<150);
        QTest::qWait(200); QVERIFY(media->position()<=2100);
        out->setValue(2.04);
        media->seek(2040); QTRY_VERIFY(qAbs(media->position()-2100)<50);
        QTest::qWait(150);
        media->seek(1500); media->pause(false);
        QTRY_VERIFY(media->position()>=1500 && media->position()<2000);
        QTRY_VERIFY_WITH_TIMEOUT(media->paused(),3000);
        media->seek(1500); QTRY_VERIFY(qAbs(media->position()-1500)<50);
        QTest::qWait(150); QVERIFY(qAbs(media->position()-1500)<50);
        player.findChild<QPushButton *>("clipTrim")->click();
        media->seek(7000); QTRY_VERIFY(qAbs(media->position()-7000)<150);
        const auto output=qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if(!output.isEmpty()) QVERIFY(timeline->grab().save(output+".timeline.png"));
    }
    void playerInitializesWithRegionalNumericLocale() {
        const QByteArray previous=std::setlocale(LC_NUMERIC,nullptr);
        const auto restore=qScopeGuard([previous] { std::setlocale(LC_NUMERIC,previous.constData()); });
        if(!std::setlocale(LC_NUMERIC,"de_DE.UTF-8"))
            QSKIP("The de_DE.UTF-8 comma-decimal locale is required for this check.");
        QCOMPARE(QByteArray(std::localeconv()->decimal_point),QByteArray(","));
        MpvPlayer player; player.show(); QSignalSpy errors(&player,&MpvPlayer::error);
        player.open("/nonexistent-shadowless-locale-test.mp4");
        QCOMPARE(QByteArray(std::setlocale(LC_NUMERIC,nullptr)),QByteArray("C"));
        for(const auto &error:errors) QVERIFY(!error[0].toString().contains("create the media player"));
        player.release();
    }
    void playerLoadsSidebarThumbnailsAfterLeavingLibrary() {
        QTemporaryDir directory; const auto path=directory.filePath("red.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v","error","-f","lavfi","-i","color=red:s=160x90:r=10","-t","2","-c:v","mpeg4",path}),0);
        Dashboard dashboard(caps); dashboard.show();
        auto *library=dashboard.findChild<Library *>();
        auto *player=dashboard.findChild<ClipPlayer *>();
        connect(library,&Library::clipsChanged,this,[library,path](const QStringList &paths) { if(paths.contains(path)) library->clipRequested(path); });
        dashboard.refreshLibrary(directory.path());
        QTRY_VERIFY(player->isVisible()); QVERIFY(!library->isVisible());
        auto *recent=player->findChild<QListWidget *>("clipRecent");
        QTRY_COMPARE(recent->count(),1);
        QTRY_VERIFY(recent->item(0)->icon().pixmap(48,28).toImage().pixelColor(24,14).red()>180);
    }
    void playerNavigationFullscreenAndLazyEditor() {
        QTemporaryDir directory;
        const auto newest = directory.filePath("new.mp4"), oldest = directory.filePath("old.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=10", "-t", "4", "-c:v", "mpeg4", newest}), 0);
        QVERIFY(QFile::copy(newest, oldest));
        Dashboard dashboard(caps); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(newest);
        player->setClips({newest, oldest});
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        QSignalSpy metadata(tools, &MediaTools::metadataReady);
        QTRY_VERIFY(media->loaded());
        QVERIFY(!tools->busy()); QCOMPARE(metadata.size(), 0);
        QVERIFY(!player->findChild<QPushButton *>("clipNext")->isEnabled());
        QVERIFY(player->findChild<QPushButton *>("clipPrevious")->isEnabled());
        player->findChild<QPushButton *>("clipFullscreen")->click();
        QVERIFY(player->fullscreen());
        QCOMPARE(dashboard.findChild<QWidget *>("shadowlessPanel")->geometry(), dashboard.rect());
        QVERIFY(!player->findChild<QPushButton *>("clipTrim")->isVisible());
        QVERIFY(!player->findChild<QListWidget *>()->isVisible());
        dashboard.escape(); QVERIFY(!player->fullscreen()); QVERIFY(player->isVisible());
        player->findChild<QPushButton *>("clipPrevious")->click();
        QTRY_VERIFY(media->loaded()); QCOMPARE(media->source(), oldest);
        QVERIFY(!player->findChild<QPushButton *>("clipPrevious")->isEnabled());
        auto *recent=player->findChild<QListWidget *>("clipRecent");
        recent->setFocus(); QTest::keyClick(recent,Qt::Key_Up); QCOMPARE(recent->currentRow(),0);
        QTest::qWait(250); QCOMPARE(recent->currentRow(),0);
        QTest::keyClick(recent,Qt::Key_Return); QTRY_COMPARE(media->source(),newest);
        auto *speed = player->findChild<QComboBox *>("clipSpeed");
        QTest::keyClick(media, Qt::Key_Up); QCOMPARE(speed->currentData().toDouble(), 1.25);
        player->findChild<QPushButton *>("clipTrim")->click();
        QVERIFY(tools->busy());
        dashboard.escape();
        QTRY_VERIFY(!tools->busy()); QVERIFY(media->source().isEmpty());
        QTest::qWait(100); QCOMPARE(metadata.size(), 0);
    }
    void playerAudioEditorAndPickerLifecycle() {
        QTemporaryDir directory;
        const auto path = directory.filePath("Four audio tracks.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x180:r=15", "-f", "lavfi", "-i", "sine=frequency=440", "-f", "lavfi", "-i", "sine=frequency=880", "-f", "lavfi", "-i", "sine=frequency=1320", "-f", "lavfi", "-i", "sine=frequency=1760", "-t", "8", "-map", "0:v", "-map", "1:a", "-map", "2:a", "-map", "3:a", "-map", "4:a", "-metadata:s:a:0", "title=Game", "-metadata:s:a:1", "title=Music", "-metadata:s:a:2", "title=PC", "-metadata:s:a:3", "title=Microphone", "-c:v", "libx264", "-threads", "1", "-c:a", "aac", path}), 0);
        Dashboard dashboard(caps); dashboard.resize(1280, 900); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        QTRY_VERIFY(media->loaded()); media->setMuted(true);
        QTRY_COMPARE(media->audioTracks().size(), 4);
        QTRY_COMPARE(player->findChildren<QCheckBox *>(QRegularExpression("clipTrack.*")).size(), 4);
        QVERIFY(!tools->busy());
        media->pause(true); QTRY_VERIFY(media->paused());
        auto *second = player->findChild<QCheckBox *>("clipTrack1"); QVERIFY(second); second->setChecked(false);
        QTest::qWait(100); QVERIFY(!second->isChecked());
        player->findChild<QPushButton *>("clipTrim")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(), 15000);
        auto *audio = player->findChild<QScrollArea *>("clipAudioTracks"); QVERIFY(audio);
        for (int i=0;i<4;++i) {
            auto *track=player->findChild<QCheckBox *>(QString("clipTrack%1").arg(i)); QVERIFY(track);
            QVERIFY(track->isVisible());
            QVERIFY2(audio->viewport()->rect().contains(QRect(track->mapTo(audio->viewport(),QPoint()),track->size())), "Every track toggle must fit without scrolling");
            QVERIFY(track->height() >= 32);
        }
        auto *exportButton = player->findChild<QPushButton *>("clipExport");
        QVERIFY(exportButton->isEnabled());
        auto *in = player->findChild<QDoubleSpinBox *>("clipIn"), *out = player->findChild<QDoubleSpinBox *>("clipOut");
        in->setFocus(); in->selectAll(); QTest::keyClicks(in,"1.5"); QTest::keyClick(in,Qt::Key_Return);
        QCOMPARE(in->value(),1.5); QCOMPARE(in->text(),QString("00:01.500"));
        in->setFocus(); in->selectAll(); QTest::keyClicks(in,"00:01.250"); QTest::keyClick(in,Qt::Key_Return);
        QCOMPARE(in->value(),1.25); QCOMPARE(in->text(),QString("00:01.250"));
        out->setValue(3); QVERIFY(exportButton->isEnabled());
        const int rowHeight=second->parentWidget()->height();
        QTest::mouseClick(second,Qt::LeftButton,Qt::NoModifier,QPoint(second->width()-8,second->height()/2));
        QVERIFY(second->isChecked()); QTest::qWait(200);
        QCOMPARE(second->parentWidget()->height(),rowHeight);
        second->setFocus(); QTest::keyClick(second,Qt::Key_Space); QVERIFY(!second->isChecked());
        QTest::qWait(200); QCOMPARE(second->parentWidget()->height(),rowHeight);
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) QVERIFY(dashboard.findChild<QWidget *>("shadowlessPanel")->grab().save(output + ".editor.png"));
        player->setFullscreen(true); QVERIFY(!in->isVisible());
        dashboard.escape(); QVERIFY(in->isVisible());
        exportButton->click();
        QTRY_VERIFY(player->findChild<QFileDialog *>()->isVisible());
        dashboard.escape(); QVERIFY(player->isVisible()); QVERIFY(in->isVisible());
        dashboard.escape(); QVERIFY(media->source().isEmpty()); QVERIFY(!tools->busy());
    }
    void playerExportSurvivesHidingOverlay_data() {
        QTest::addColumn<int>("outcome");
        QTest::newRow("complete") << 0;
        QTest::newRow("cancel") << 1;
        QTest::newRow("failure") << 2;
    }
    void playerExportSurvivesHidingOverlay() {
        QFETCH(int, outcome);
        QTemporaryDir directory;
        const auto path = directory.filePath("source.mp4"), output = directory.filePath("source_trim.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x180:r=15", "-t", "4", "-c:v", "libx264", "-threads", "1", path}), 0);
        Dashboard dashboard(caps); dashboard.resize(1280, 900); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        dashboard.findChild<Library *>()->clipRequested(path);
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        QTRY_VERIFY(media->loaded());
        player->findChild<QPushButton *>("clipTrim")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(), 15000);
        player->findChild<QPushButton *>("clipExport")->click();
        auto *picker = player->findChild<QFileDialog *>(); QVERIFY(picker);
        QCOMPARE(picker->selectedFiles().value(0), output);
        QSignalSpy exported(tools, &MediaTools::exported), cancelled(tools, &MediaTools::cancelled), failed(tools, &MediaTools::failed);
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QVERIFY(tools->busy());
        auto *rendering = dashboard.findChild<QLabel *>("clipRenderingStatus");
        QVERIFY(rendering); QCOMPARE(rendering->text(), "rendering...");
        QVERIFY(rendering->isVisible());
        dashboard.hide(); dashboard.home();
        QVERIFY(media->source().isEmpty());
        QCOMPARE(cancelled.size(), 0);
        dashboard.show();
        QVERIFY(rendering->isVisible());
        dashboard.findChild<Library *>()->clipRequested(path);
        QCOMPARE(cancelled.size(), 0);
        if (outcome) {
            if (outcome == 1) tools->stop();
            else QVERIFY(QFile::remove(path));
            QTRY_VERIFY_WITH_TIMEOUT(!tools->busy(), 15000);
            QVERIFY(!rendering->isVisible());
            QCOMPARE(exported.size(), 0);
            QCOMPARE(cancelled.size(), outcome == 1 ? 1 : 0);
            QCOMPARE(failed.size(), outcome == 2 ? 1 : 0);
            QVERIFY(!QFileInfo::exists(output));
            return;
        }
        QTRY_COMPARE_WITH_TIMEOUT(exported.size(), 1, 15000);
        QVERIFY(!rendering->isVisible());
        QCOMPARE(exported[0][0].toString(), output);
        QVERIFY(QFileInfo::exists(output));
        QCOMPARE(QProcess::execute("ffmpeg", {"-v", "error", "-i", output, "-f", "null", "-"}), 0);
    }
    void playerResourceSample() {
        const auto source = qEnvironmentVariable("SHADOWLESS_PLAYER_SAMPLE");
        if (source.isEmpty()) QSKIP("Set SHADOWLESS_PLAYER_SAMPLE to a generated fixture for the opt-in resource sample.");
        const auto output = qEnvironmentVariable("SHADOWLESS_PLAYER_SAMPLE_OUTPUT");
        QVERIFY(!output.isEmpty());
        const auto mark = [&](const QByteArray &phase) { QFile file(output + ".phase"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(phase); };
        Dashboard dashboard(caps); dashboard.resize(1280, 720); dashboard.show();
        auto *player = dashboard.findChild<ClipPlayer *>();
        auto *media = player->findChild<MpvPlayer *>();
        auto *tools = player->findChild<MediaTools *>();
        mark("idle"); QTest::qWait(3500);
        dashboard.findChild<Library *>()->clipRequested(source); media->setMuted(true);
        QTRY_VERIFY(media->loaded()); QTRY_VERIFY(media->position() > 100);
        QSignalSpy frames(media, &MpvPlayer::frameRendered);
        mark("playing"); QTest::qWait(4000);
        const int playingFrames = frames.size(); QVERIFY(playingFrames > 20); QVERIFY(!tools->busy());
        const auto decoder = media->hardwareDecoder();
        dashboard.hide(); QVERIFY(media->source().isEmpty()); QTest::qWait(250);
        const int hiddenFrames = frames.size();
        mark("hidden"); QTest::qWait(3500);
        QCOMPARE(frames.size(), hiddenFrames); QVERIFY(!tools->busy());
        QFile result(output + ".json"); QVERIFY(result.open(QIODevice::WriteOnly));
        result.write(QJsonDocument(QJsonObject{{"playing_frames", playingFrames}, {"hidden_frames", frames.size() - hiddenFrames}, {"analysis_running", tools->busy()}, {"hardware_decoder", decoder}}).toJson());
        mark("done");
    }
    void showsRecordingAlongsideReplay() {
        Dashboard dashboard(caps);
        dashboard.show();
        dashboard.updateState(GsrOptions{}, {}, true, true, false, false);
        const auto recording = button(&dashboard, "Recording");
        QVERIFY(recording && recording->isVisible() && recording->isEnabled());
        QVERIFY(dashboard.findChild<QPushButton *>("captureAction")->isEnabled());
    }
    void exposesCaptureDeviceSelectors() {
        SettingsPage page(caps);
        const auto output = page.findChild<QComboBox *>("audioOutput");
        const auto input = page.findChild<QComboBox *>("audioInput");
        QVERIFY(output && input);
        QCOMPARE(output->currentText(), "Default output");
        QCOMPARE(input->currentText(), "Default input");
    }
    void savesPcTrackFromAudioSettings() {
        SettingsPage page(caps); GsrOptions options;
        options.source = "DP-1"; options.outputDirectory = "/tmp/clips";
        options.applicationAudio = true; options.applicationTracks = {{"Game", 1}, {"Spotify", 2}};
        options.pcTrack = 3; page.edit(options, false, false, keys); page.showAudio(); page.show();
        auto *pc = page.findChild<QComboBox *>("pcAudioTrack"); QVERIFY(pc);
        QVERIFY(pc->isVisible()); QCOMPARE(pc->currentIndex(), 3);
        pc->setCurrentIndex(5);
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 1);
        const auto result = qvariant_cast<GsrOptions>(saved[0][0]); QCOMPARE(result.pcTrack, 5);
        QCOMPARE(result.applicationTracks, options.applicationTracks);
    }
    void videoPresetsPreserveCustomValuesAndIgnoreWheel() {
        SettingsPage page(caps);
        GsrOptions original;
        original.source = "DP-1";
        original.outputDirectory = "/tmp/clips";
        original.frameRate = 75;
        original.bitrateMbps = 27;
        page.edit(original, false, false, keys);

        for (const auto *name : {"captureFps", "captureBitrate"}) {
            auto *selector = page.findChild<QComboBox *>(name);
            QVERIFY(selector);
            QVERIFY(!selector->isEditable());
            QCOMPARE(selector->currentText(), "Custom");
            selector->setCurrentIndex(1);
            QWheelEvent wheel(QPointF(4, 4), QPointF(4, 4), {}, QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(selector, &wheel);
            QCOMPARE(selector->currentIndex(), 1);
            selector->setCurrentIndex(selector->count() - 1);
        }

        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1);
        const auto options = qvariant_cast<GsrOptions>(saved.first().first());
        QCOMPARE(options.frameRate, 75);
        QCOMPARE(options.bitrateMbps, 27);
    }
    void migratesLegacySettingsAndPersistsAccent() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        QSettings legacy("Captail", "LinuxGsrSpike");
        legacy.setValue("fps", 75);
        legacy.setValue("outputDirectory", "/tmp/existing-clips");
        legacy.sync();

        Settings settings;
        const auto options = settings.capture(caps);
        QCOMPARE(options.frameRate, 75);
        QCOMPARE(options.outputDirectory, "/tmp/existing-clips");
        QCOMPARE(settings.accent(), "mint");

        SettingsPage page(caps);
        page.edit(options, false, false, keys, settings.accent());
        const auto selector = page.findChild<QComboBox *>("accentColor");
        QVERIFY(selector);
        QCOMPARE(selector->count(), 5);
        selector->setCurrentIndex(selector->findData("violet"));
        QSignalSpy saved(&page, &SettingsPage::saved);
        button(&page, "Save settings")->click();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first()[3].toString(), "violet");

        QString error;
        QVERIFY(settings.save(options, {}, false, &error, keys, saved.first()[3].toString()));
        QCOMPARE(Settings().accent(), "violet");
        QCOMPARE(legacy.value("fps").toInt(), 75);
        Dashboard dashboard(caps);
        dashboard.setAccent(Settings().accent());
        QCOMPARE(dashboard.palette().color(QPalette::Accent), QColor("#b69af7"));
        QVERIFY(!dashboard.windowIcon().pixmap(22, 22).isNull());
    }
    void dashboardKeepsRecordingVolumeAvailableDuringCapture() {
        Dashboard dashboard(caps);
        GsrOptions options;
        options.levels.systemVolume = 63;
        options.levels.systemMuted = true;
        dashboard.updateState(options, {}, true, true, false, false);
        const auto system = button(&dashboard, "System audio");
        QVERIFY(system && system->isEnabled() && !system->isChecked());
        QSignalSpy levels(&dashboard, &Dashboard::volumeChanged);
        for (auto *slider : dashboard.findChildren<QSlider *>()) {
            if (slider->accessibleName() != "System audio capture volume") continue;
            QCOMPARE(slider->value(), 63);
            slider->setValue(46);
        }
        QCOMPARE(levels.size(), 1);
        QCOMPARE(levels[0][0].toBool(), false);
        QCOMPARE(levels[0][1].toInt(), 46);
    }
    void savesAudioDevicesAndMixAcrossLaunches() {
        QTemporaryDir directory;
        const auto file = directory.filePath("settings.conf");
        GsrOptions options;
        options.audioOutput = "speakers.monitor";
        options.audioInput = "mic";
        options.applicationAudio = true;
        options.microphoneTrack = 4;
        options.pcTrack = 3;
        options.applicationTracks = {{"Game", 1}, {"Voice chat", 4}};
        options.levels.microphoneMuted = true;
        options.levels.systemVolume = 73;
        QString error;
        QVERIFY(Settings(file).save(options, {}, false, &error));
        const auto restored = Settings(file).capture(caps);
        QCOMPARE(restored.audioOutput, "speakers.monitor");
        QCOMPARE(restored.audioInput, "mic");
        QCOMPARE(restored.applicationTracks, options.applicationTracks);
        QCOMPARE(restored.microphoneTrack, 4);
        QCOMPARE(restored.pcTrack, 3);
        QCOMPARE(restored.levels.systemVolume, 73);
        QVERIFY(restored.applicationAudio && restored.levels.microphoneMuted);
    }
    void dashboardEscapeReturnsBeforeHiding() {
        Dashboard dashboard(caps); dashboard.resize(1280, 720); dashboard.show();
        QSignalSpy hidden(&dashboard, &Dashboard::hideRequested);
        QTest::mouseClick(dashboard.findChild<QPushButton *>("openSettings"), Qt::LeftButton);
        dashboard.escape(); QCOMPARE(hidden.size(), 0);
        dashboard.escape(); QCOMPARE(hidden.size(), 1);
        dashboard.updateState(GsrOptions{}, OverlayScene{}, true, true, false, false);
        QVERIFY(button(&dashboard, "Instant Clip")->isEnabled());
        const auto output = qEnvironmentVariable("SHADOWLESS_UI_OUTPUT");
        if (!output.isEmpty()) {
            GsrOptions options;
            options.source = "DP-1";
            options.outputDirectory = qEnvironmentVariable("SHADOWLESS_UI_FIXTURES");
            dashboard.updateState(options, {}, true, true, false, false);
            auto *library = dashboard.findChild<Library *>();
            dashboard.hide();
            dashboard.refreshLibrary(options.outputDirectory);
            QSignalSpy scanned(library, &Library::storageChanged);
            for (int pass = 1; pass <= 3; ++pass) {
                dashboard.hide();
                dashboard.refreshLibrary(options.outputDirectory);
                dashboard.show();
                QTRY_COMPARE(scanned.size(), pass);
                if (!options.outputDirectory.isEmpty()) {
                    const auto thumbnails = library->findChildren<QLabel *>("thumbnail");
                    QVERIFY(!thumbnails.isEmpty());
                    QTRY_VERIFY(std::all_of(thumbnails.cbegin(), thumbnails.cend(), [](auto *thumbnail) {
                        return !thumbnail->pixmap().isNull();
                    }));
                }
            }
            auto *panel = dashboard.findChild<QWidget *>("shadowlessPanel");
            QVERIFY(panel->grab().save(output));
            dashboard.settingsPage()->edit(options, false, false, keys);
            dashboard.findChild<QPushButton *>("openSettings")->click();
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".settings.png"));
            dashboard.home();
            button(&dashboard, "Audio settings")->click();
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".audio.png"));
            dashboard.home();
            dashboard.findChild<QPushButton *>("openOverlays")->click();
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".overlays.png"));
            dashboard.home();
            dashboard.resize(740, 720);
            QCoreApplication::processEvents();
            QVERIFY(panel->grab().save(output + ".narrow.png"));
        }
    }
    void dashboardModeButtonsToggleCaptureWithoutSelectingTabs() {
        Dashboard dashboard(caps); dashboard.show();
        GsrOptions options; options.source = "DP-1"; options.bufferSeconds = 4;
        auto *replay = button(&dashboard, "Instant Clip");
        auto *record = button(&dashboard, "Recording");
        auto *action = dashboard.findChild<QPushButton *>("captureAction");
        QVERIFY(replay && record && action);
        QVERIFY(!dashboard.findChild<QPushButton *>("captureToggle"));
        QSignalSpy modes(&dashboard, &Dashboard::recordingMode);
        QSignalSpy saved(&dashboard, &Dashboard::saveReplay);
        dashboard.updateState(options, {}, false, false, false, false);
        QVERIFY(!replay->isChecked() && !record->isChecked());
        replay->click();
        QCOMPARE(modes.size(), 1); QCOMPARE(modes.last().at(0).toBool(), false);
        QVERIFY(!replay->isChecked());
        record->click();
        QCOMPARE(modes.size(), 2); QCOMPARE(modes.last().at(0).toBool(), true);
        dashboard.updateState(options, {}, true, false, false, false);
        QVERIFY(replay->isEnabled() && !record->isEnabled());
        replay->click(); QCOMPARE(modes.size(), 3);
        dashboard.updateState(options, {}, true, true, false, false);
        QVERIFY(replay->isChecked() && !record->isChecked());
        QVERIFY(replay->isEnabled() && record->isEnabled());
        action->click(); QCOMPARE(saved.size(), 1); QCOMPARE(action->text(), "Save 4 sec");
        dashboard.updateState(options, {}, true, true, false, false, true);
        QVERIFY(replay->isChecked() && record->isChecked());
        record->click(); QCOMPARE(modes.size(), 4); QVERIFY(record->isChecked());
        dashboard.updateState(options, {}, true, true, false, false, true, true);
        QVERIFY(!replay->isEnabled() && !record->isEnabled());
        options.continuousRecording = true;
        dashboard.updateState(options, {}, true, true, false, false);
        QVERIFY(!replay->isChecked() && record->isChecked());
        QVERIFY(!replay->isEnabled() && record->isEnabled());
        QVERIFY(!action->isVisible());
        record->click(); QCOMPARE(modes.size(), 5);
        dashboard.updateState(options, {}, true, true, false, true);
        QVERIFY(!replay->isEnabled() && !record->isEnabled());
    }
    void overlayShortcutHidesSettingsAndEditor() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = directory.path();
        QString error;
        QVERIFY(Settings().save(options, {}, false, &error, {"Ctrl+J", keys[1], keys[2]}));
        Controller controller(caps, {}, false);
        Dashboard *dashboard = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *candidate = qobject_cast<Dashboard *>(widget)) dashboard = candidate;
        }
        QVERIFY(dashboard);
        auto *settings = dashboard->findChild<QPushButton *>("openSettings");
        auto *overlays = dashboard->findChild<QPushButton *>("openOverlays");
        settings->click();
        QVERIFY(dashboard->settingsPage()->isVisible());
        auto *shortcutField = dashboard->settingsPage()->findChild<QKeySequenceEdit *>();
        QVERIFY(shortcutField);
        shortcutField->setFocus();
        QTest::keyClick(shortcutField, Qt::Key_J, Qt::ControlModifier);
        QVERIFY(!dashboard->isVisible());
        controller.command("toggle");
        QVERIFY(overlays->isVisible());
        overlays->click();
        button(dashboard->overlayEditor(), "Browse…")->click();
        auto *picker = dashboard->overlayEditor()->findChild<QFileDialog *>();
        QVERIFY(picker->isVisible());
        QTest::keyClick(picker, Qt::Key_J, Qt::ControlModifier);
        QVERIFY(!dashboard->isVisible());
        controller.command("toggle");
        QVERIFY(overlays->isVisible());
        overlays->click();
        QVERIFY(!picker->isVisible());
    }
    void restoresAltZAfterChangingOverlayShortcut() {
        Dashboard dashboard(caps);
        GsrOptions options;
        options.source = "DP-1";
        options.outputDirectory = "/tmp/clips";
        dashboard.show();
        QSignalSpy saved(dashboard.settingsPage(), &SettingsPage::saved);
        for (const auto &combination : {QString("Alt+O"), QString("Alt+Z")}) {
            const auto current = saved.isEmpty() ? keys : saved.last()[2].toStringList();
            dashboard.setShortcutKeys(current);
            dashboard.settingsPage()->edit(options, false, false, current);
            dashboard.findChild<QPushButton *>("openSettings")->click();
            auto *field = dashboard.settingsPage()->findChild<QKeySequenceEdit *>();
            field->clear();
            field->setFocus();
            QTest::keySequence(field, QKeySequence(combination));
            QCOMPARE(field->keySequence(), QKeySequence(combination));
            const int previous = saved.size();
            button(dashboard.settingsPage(), "Save settings")->click();
            QCOMPARE(saved.size(), previous + 1);
            QCOMPARE(saved.last()[2].toStringList().first(), combination);
            dashboard.home();
        }
    }
    void indicatorAnimatesOnlyWhenVisibleAndConfirmsSave() {
        StatusIndicator indicator;
        auto *animation = indicator.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        indicator.show();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        const auto firstFrame = indicator.grab().toImage();
        QTest::qWait(120);
        QVERIFY(firstFrame != indicator.grab().toImage());
        indicator.saved();
        QCOMPARE(indicator.accessibleDescription(), QString("Clip saved"));
        indicator.setState(StatusIndicator::Recording);
        QCOMPARE(indicator.accessibleDescription(), QString("Clip saved"));
        QTRY_COMPARE(indicator.accessibleDescription(), QString("Recording"));
        indicator.hide();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        indicator.show();
        indicator.saved();
        indicator.hide();
        indicator.show();
        QCOMPARE(indicator.accessibleDescription(), QString("Recording"));
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
    }
    void overlayCloseButtonReopensDashboard() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        Controller controller(caps, {}, false);
        Dashboard *dashboard = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *candidate = qobject_cast<Dashboard *>(widget)) dashboard = candidate;
        }
        QVERIFY(dashboard);
        auto *overlays = dashboard->findChild<QPushButton *>("openOverlays");
        overlays->click();
        QVERIFY(dashboard->overlayEditor()->isVisible());
        dashboard->findChild<QPushButton *>("hideOverlay")->click();
        QVERIFY(!dashboard->isVisible());
        controller.command("toggle");
        QVERIFY(overlays->isVisible());
    }
    void settingsCancelAndValidationPreserveStagedValues() {
        SettingsPage page(caps); GsrOptions original; original.source = "DP-1"; original.outputDirectory = "/tmp/clips";
        original.bitrateMbps = 35; page.edit(original, false, false, keys);
        QSignalSpy saved(&page, &SettingsPage::saved), cancelled(&page, &SettingsPage::cancelled);
        auto *cancel = page.findChild<QPushButton *>("cancelSettings"); QVERIFY(cancel); cancel->click(); QCOMPARE(cancelled.size(), 1); QCOMPARE(saved.size(), 0);
        auto *fps = page.findChild<QComboBox *>("captureFps");
        QVERIFY(fps);
        QCOMPARE(fps->currentData().toInt(), 60);
        page.edit(original, false, true, keys); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 1);
        const auto options = qvariant_cast<GsrOptions>(saved[0][0]); QCOMPARE(options.bitrateMbps, 35); QCOMPARE(options.source, "DP-1");
        page.edit(original, false, false, keys); fps->setCurrentIndex(fps->findData(144));
        page.setCaptureActive(true); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 2);
        QCOMPARE(qvariant_cast<GsrOptions>(saved[1][0]).frameRate, 60);
        page.edit(original, false, false, keys);
        const auto keyEdits = page.findChildren<QKeySequenceEdit *>(); QCOMPARE(keyEdits.size(), 3);
        keyEdits[1]->setKeySequence(QKeySequence("Alt+Z")); button(&page, "Save settings")->click(); QCOMPARE(saved.size(), 2);
    }
    void editorFlushOnlyPublishesEditedScene() {
        QTemporaryDir directory; const auto path = directory.filePath("image with spaces.png");
        QImage image(160, 90, QImage::Format_RGBA8888); image.fill(Qt::red); QVERIFY(image.save(path));
        OverlayEditor editor; QSignalSpy changed(&editor, &OverlayEditor::changed);
        editor.flush(); QCOMPARE(changed.size(), 0);
        OverlayScene scene; scene.enabled = true; editor.edit(scene);
        QLineEdit *pathField = nullptr; for (auto *field : editor.findChildren<QLineEdit *>()) if (field->accessibleName() == "Overlay media path") pathField = field;
        QVERIFY(pathField); pathField->setText(path); button(&editor, "Add local media")->click();
        editor.flush(); QCOMPARE(changed.size(), 1);
        const auto applied = qvariant_cast<OverlayScene>(changed[0][0]); QCOMPARE(applied.layers.size(), 1); QCOMPARE(applied.layers[0].path, path);
        editor.flush(); QCOMPARE(changed.size(), 1);
        const auto picker = editor.findChild<QFileDialog *>(); QVERIFY(picker); QVERIFY(!picker->isWindow());
    }
    void dashboardOverlaySwitchPersistsConfirmedState() {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        Controller controller(caps, {}, true);
        auto *dashboard = [&]() -> Dashboard * {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (auto *candidate = qobject_cast<Dashboard *>(widget)) return candidate;
            }
            return nullptr;
        }();
        QVERIFY(dashboard);
        auto *toggle = dashboard->findChild<QPushButton *>("overlayToggle");
        auto *editorButton = dashboard->findChild<QPushButton *>("openOverlays");
        QVERIFY(toggle);
        QVERIFY(editorButton);
        QVERIFY(!toggle->isChecked());
        toggle->click();
        QVERIFY(!toggle->isEnabled());
        QVERIFY(!editorButton->isEnabled());
        QTRY_VERIFY(toggle->isEnabled());
        QVERIFY(editorButton->isEnabled());
        QVERIFY(toggle->isChecked());
        QVERIFY(Settings().scene().enabled);
        toggle->click();
        QTRY_VERIFY(toggle->isEnabled());
        QVERIFY(!toggle->isChecked());
        QVERIFY(!Settings().scene().enabled);
    }
    void inactiveControllerRejectsMissingMedia() {
        QTemporaryDir directory; QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
        OverlayScene original; original.enabled = true; OverlayLayer layer; layer.path = directory.filePath("missing.png"); original.layers.append(layer);
        GsrOptions options; options.source = "DP-1"; options.outputDirectory = directory.path(); QString error;
        Settings settings; QVERIFY(settings.save(options, original, false, &error));
        Controller controller(caps, {}, true);
        Dashboard *dashboard = nullptr; for (auto *widget : QApplication::topLevelWidgets()) if (widget->objectName() == "dashboard") dashboard = qobject_cast<Dashboard *>(widget);
        QVERIFY(dashboard); auto *editor = dashboard->overlayEditor(); editor->edit(original);
        auto *opacity = editor->findChild<QSpinBox *>(); QVERIFY(opacity); opacity->setValue(23); editor->flush();
        auto failed = [&] { for (auto *label : editor->findChildren<QLabel *>()) if (label->text().contains("Cannot read media")) return true; return false; };
        QTRY_VERIFY(failed());
        QCOMPARE(Settings().scene().layers[0].opacity, 100);
    }
    void controllerReportsPersistenceFailure() {
        QTemporaryDir directory; const auto blocked = directory.filePath("regular-file");
        QFile file(blocked); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("blocked"); file.close();
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, blocked);
        Controller controller(caps, {}, true);
        Dashboard *dashboard = nullptr; for (auto *widget : QApplication::topLevelWidgets()) if (widget->objectName() == "dashboard") dashboard = qobject_cast<Dashboard *>(widget);
        QVERIFY(dashboard); auto *editor = dashboard->overlayEditor(); editor->edit({});
        QCheckBox *master = nullptr; for (auto *check : editor->findChildren<QCheckBox *>()) if (check->text() == "Enable composition") master = check;
        QVERIFY(master); master->setChecked(true); editor->flush();
        auto failed = [&] { for (auto *label : editor->findChildren<QLabel *>()) if (label->text().contains("not saved")) return true; return false; };
        QTRY_VERIFY(failed());
        for (auto *label : editor->findChildren<QLabel *>()) QVERIFY(label->text() != "Layers applied and saved.");
    }
};
QTEST_MAIN(UiTests)
#include "tst_ui.moc"
