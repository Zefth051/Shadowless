#include <QFile>
#include <QRegularExpression>
#include <QtTest>

class CaptureIndependence : public QObject {
    Q_OBJECT
private slots:
    void startupReferences_data() {
        QTest::addColumn<QString>("path");
        for (const char *path : {"app/main.cpp", "app/controller.cpp", "app/controller.h",
                                 "capture/gsr_capture.cpp", "capture/gsr_capture.h",
                                 "capture/audio.cpp", "capture/audio.h", "run.sh",
                                 "engine/launch-recorder", "packaging/AppRun"})
            QTest::newRow(path) << QString::fromLatin1(path);
    }

    void startupReferences() {
        QFETCH(QString, path);
        const auto relative = "../../" + path;
        QFile source(QFINDTESTDATA(qPrintable(relative)));
        QVERIFY2(source.open(QIODevice::ReadOnly), qPrintable(relative));

        // Controller observes inputAccessChanged to retry the preview. It must
        // never acquire the grant API or launch the privileged input helper.
        QString forbidden = R"(overlays/input/access/|input_access|\bInputAccess(?:Dialog)?\b|input[-_]setup|/dev/input|\bpkexec\b)";
        if (!path.startsWith("app/controller."))
            forbidden += R"(|overlays/input/|evdev_source|EvdevInputSource|input_sidecar_session|InputSidecarSession)";
        const QRegularExpression access(forbidden);
        int line = 0;
        while (!source.atEnd()) {
            ++line;
            const auto match = access.match(QString::fromUtf8(source.readLine()));
            QVERIFY2(!match.hasMatch(), qPrintable(QString("%1:%2 references %3")
                .arg(path).arg(line).arg(match.captured())));
        }
    }
};

QTEST_APPLESS_MAIN(CaptureIndependence)
#include "tst_capture_independence.moc"
