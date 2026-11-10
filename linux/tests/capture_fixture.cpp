#include "../settings/settings.h"
#include <QApplication>
#include <QWidget>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <cstdio>

class Fixture : public QWidget {
    QElapsedTimer m_clock;
    QTimer m_timer;
    int m_keys = 0;
public:
    Fixture() {
        setWindowTitle("Shadowless capture fixture"); m_clock.start(); m_timer.setInterval(16);
        connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update)); m_timer.start();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.fillRect(rect(), QColor("#17242b"));
        p.fillRect(QRect((m_clock.elapsed() / 8) % qMax(1, width() - 80), height() / 2, 80, 80), QColor("#63e0bd"));
        p.setPen(Qt::white); p.setFont(QFont("sans-serif", 24));
        p.drawText(rect().adjusted(48, 48, -48, -48), Qt::AlignTop | Qt::AlignLeft,
            QString("Shadowless fullscreen fixture\n%1 ms · %2 received keys\nAlt+Z opens the overlay; A checks restored keyboard input.").arg(m_clock.elapsed()).arg(m_keys));
    }
    void keyPressEvent(QKeyEvent *event) override { ++m_keys; fprintf(stdout, "key=%d count=%d\n", event->key(), m_keys); fflush(stdout); }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv); app.setApplicationName("shadowless-capture-fixture");
    if (app.arguments().contains("--configure")) {
        const auto args = app.arguments(); const int index = args.indexOf("--configure");
        if (args.size() < index + 3) return 1;
        Shadowless::Settings settings; Shadowless::GsrOptions options;
        options.source = "DP-1"; options.outputDirectory = args[index + 1]; options.systemAudio = false;
        Shadowless::OverlayScene scene; scene.enabled = true;
        Shadowless::OverlayLayer layer; layer.path = args[index + 2]; layer.x = .1; layer.y = .1;
        scene.layers.append(layer); QString error;
        return settings.save(options, scene, true, &error) ? 0 : 1;
    }
    Fixture window; window.showFullScreen(); return app.exec();
}
