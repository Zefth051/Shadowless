#pragma once
#include "../capture/gsr_capture.h"
#include "../overlays/layer.h"
#include <QSettings>

namespace Shadowless {
class Settings {
public:
    explicit Settings(const QString &path = {});
    GsrOptions capture(const GsrCapabilities &capabilities) const;
    OverlayScene scene() const;
    bool indicator() const;
    QString accent() const;
    QStringList shortcutKeys() const;
    bool save(const GsrOptions &options, const OverlayScene &scene, bool indicator, QString *error,
              const QStringList &keys = {"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"}, const QString &accent = {});
private:
    std::unique_ptr<QSettings> m_store;
};
} // namespace Shadowless
