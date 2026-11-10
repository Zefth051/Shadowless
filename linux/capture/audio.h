#pragma once

#include "gsr_capture.h"
#include <QFutureWatcher>
#include <QList>
#include <atomic>

namespace Shadowless {

struct AudioDevice {
    QString id, description;
};

struct AudioCatalog {
    QList<AudioDevice> outputs, inputs;
    QStringList applications;
    QString error;
    static AudioCatalog probe(const QString &program = "gpu-screen-recorder");
};

class CaptureAudio : public QObject {
    Q_OBJECT
public:
    explicit CaptureAudio(QObject *parent = nullptr);
    ~CaptureAudio() override;
    void start(const GsrOptions &options);
    void stop();
    void setLevels(const AudioLevels &levels);

signals:
    void ready(const QString &output, const QString &input);
    void failed(const QString &message);

private:
    struct Route {
        QProcess *process = nullptr;
        QString name;
    };
    Route m_output, m_input;
    AudioLevels m_levels;
    QTimer m_debounce;
    bool m_ready = false, m_updating = false, m_dirty = false;
    int m_generation = 0;
    std::shared_ptr<std::atomic_bool> m_cancelUpdate;

    void launch(Route &route, const QString &target, bool output);
    void applyLevels();
};

} // namespace Shadowless
