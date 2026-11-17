#pragma once
#include "../capture/gsr_capture.h"
#include "../overlays/layer.h"
#include <QSettings>

namespace Shadowless {
// Peripheral (keyboard/mouse/gamepad) overlay. Everything is off by default: layout "none" means no
// input is read at all. The sidecar (timestamped allowlisted events next to each clip) is a separate opt-in.
struct InputOverlaySettings {
    QString layout = "none";          // "none" or a built-in layout id
    bool sidecar = false;
    QString corner = "bottom-left";   // top-left | top-right | bottom-left | bottom-right
    int scale = 100;                  // 50..200 percent
    int opacity = 100;                // 20..100 percent
    bool enabled() const { return layout != "none"; }
    InputOverlaySettings sanitized() const;
    bool operator==(const InputOverlaySettings &o) const {
        return layout == o.layout && sidecar == o.sidecar && corner == o.corner && scale == o.scale && opacity == o.opacity;
    }
};
class Settings {
public:
    explicit Settings(const QString &path = {});
    GsrOptions capture(const GsrCapabilities &capabilities) const;
    OverlayScene scene() const;
    bool indicator() const;
    QString accent() const;
    QString indicatorPosition() const;
    QStringList shortcutKeys() const;
    bool launchAtLogin() const;
    bool startReplayOnLaunch() const;
    bool discordPresence() const;
    void setDiscordPresence(bool enabled);
    bool idleCompression() const;
    void setIdleCompression(bool enabled);
    InputOverlaySettings inputOverlay() const;
    void setInputOverlay(const InputOverlaySettings &value);
    bool saveStartup(bool launchAtLogin, bool startReplayOnLaunch, QString *error);
    bool save(const GsrOptions &options, const OverlayScene &scene, bool indicator, QString *error,
              const QStringList &keys = {"Alt+Z", "Ctrl+Shift+F10", "Alt+F10", "Alt+F9"}, const QString &accent = {}, const QString &position = "top-right");
private:
    std::unique_ptr<QSettings> m_store;
};
} // namespace Shadowless
