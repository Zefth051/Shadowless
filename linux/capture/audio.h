#pragma once

#include "gsr_capture.h"
#include <QHash>
#include <QList>

namespace Shadowless {

struct AudioDevice {
    QString id, description;
};

struct AudioCatalog {
    QList<AudioDevice> outputs, inputs;
    QStringList applications;
    QHash<QString, QString> applicationIcons;
    QString error;
    static AudioCatalog probe(const QString &program = managedGsrProgram());
};

class CaptureAudio : public QObject {
    Q_OBJECT
public:
    explicit CaptureAudio(QObject *parent = nullptr);
    ~CaptureAudio() override;
    void start(const GsrOptions &options);
    void stop();
    void setLevels(const AudioLevels &levels);
    QString levelsPath() const { return m_runtime.filePath("audio-levels"); }

signals:
    void ready(const QString &output, const QString &input);
    void failed(const QString &message);

private:
    QTemporaryDir m_runtime;
    bool writeLevels(const AudioLevels &levels);

};

} // namespace Shadowless
