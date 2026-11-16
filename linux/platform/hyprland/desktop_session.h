#pragma once
#include <QStringList>

namespace Shadowless {
inline bool isHyprlandDesktop(const QString &desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP")) {
    return desktop.split(':', Qt::SkipEmptyParts).contains("Hyprland", Qt::CaseInsensitive);
}
inline bool isSupportedWaylandDesktop(const QString &platform, const QString &desktop) {
    const auto names = desktop.split(':', Qt::SkipEmptyParts);
    return platform == "wayland" && (isHyprlandDesktop(desktop) ||
        names.contains("KDE", Qt::CaseInsensitive) || names.contains("Plasma", Qt::CaseInsensitive));
}
} // namespace Shadowless
