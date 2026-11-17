#include "../../app/idle_compressor.h"
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace Shadowless;

class CompressionTests : public QObject {
    Q_OBJECT
    static bool makeClip(const QString &path, const QString &codec = "libx264") {
        QProcess p;
        p.start("ffmpeg", {"-y", "-v", "error", "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30:duration=2",
                           "-f", "lavfi", "-i", "sine=frequency=440:duration=2", "-c:v", codec, "-preset", "ultrafast",
                           "-crf", "18", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", path});
        return p.waitForFinished(60000) && p.exitCode() == 0;
    }
private slots:
    void initTestCase() {
        QProcess p; p.start("ffmpeg", {"-hide_banner", "-encoders"}); p.waitForFinished();
        if (!p.readAllStandardOutput().contains("libx265")) QSKIP("ffmpeg without libx265");
    }
    void candidatesSkipRecentTemporaryAndHiddenFiles() {
        QTemporaryDir dir; QDir().mkpath(dir.path() + "/Game");
        for (const auto &name : {"old.mp4", "Game/old2.mp4", "new.mp4", ".hidden.mp4", "x.shadowless-tmp.mp4", "note.txt"}) { QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); }
        const auto now = QDateTime::currentDateTime();
        for (const auto &name : {"old.mp4", "Game/old2.mp4"}) {
            QFile f(dir.filePath(name)); QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(now.addDays(-1), QFileDevice::FileModificationTime));
        }
        const auto found = IdleCompressor::candidates(dir.path(), now);
        QCOMPARE(found, (QStringList{dir.filePath("Game/old2.mp4"), dir.filePath("old.mp4")}));
    }
    void replacesH264ClipOnlyAfterValidation() {
        QTemporaryDir dir; const auto clip = dir.filePath("a.mp4"); QVERIFY(makeClip(clip));
        const auto before = QFileInfo(clip).size(); std::atomic_bool cancel{false}; QString detail;
        const auto result = IdleCompressor::compressFile(clip, cancel, &detail);
        QVERIFY2(result == IdleCompressor::Result::Compressed || result == IdleCompressor::Result::Skipped, qPrintable(detail));
        if (result == IdleCompressor::Result::Compressed) QVERIFY(QFileInfo(clip).size() < before);
        else QCOMPARE(QFileInfo(clip).size(), before); // no gain: original untouched
        QVERIFY(QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size() == 1); // no temp left
    }
    void skipsAlreadyHevcAndHonoursCancel() {
        QTemporaryDir dir; const auto hevc = dir.filePath("h.mp4"); QVERIFY(makeClip(hevc, "libx265"));
        std::atomic_bool cancel{false}; QString detail;
        QCOMPARE(IdleCompressor::compressFile(hevc, cancel, &detail), IdleCompressor::Result::Skipped);
        const auto clip = dir.filePath("c.mp4"); QVERIFY(makeClip(clip)); const auto before = QFileInfo(clip).size();
        cancel = true;
        QCOMPARE(IdleCompressor::compressFile(clip, cancel, &detail), IdleCompressor::Result::Cancelled);
        QCOMPARE(QFileInfo(clip).size(), before);
    }
    void validationRejectsMismatchedClips() {
        QTemporaryDir dir; const auto a = dir.filePath("a.mp4"), b = dir.filePath("b.mp4");
        QVERIFY(makeClip(a)); QVERIFY(makeClip(b, "libx265")); QString error;
        QVERIFY2(IdleCompressor::validate(a, b, &error), qPrintable(error));
        QVERIFY(!IdleCompressor::validate(a, a, &error)); // not HEVC
        QVERIFY(!IdleCompressor::validate(a, dir.filePath("missing.mp4"), &error));
    }
};
QTEST_MAIN(CompressionTests)
#include "tst_compression.moc"
