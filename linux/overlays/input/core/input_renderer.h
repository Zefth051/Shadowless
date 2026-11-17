#pragma once
#include "input_types.h"
#include "layout.h"
#include <QColor>
#include <QImage>
#include <QRect>

namespace Shadowless::Input {
struct RenderStyle {
    QColor accent{"#8b5cf6"}; // pressed fill
    QColor idle{10, 10, 13, 225};   // keycap base (the visible lip) and non-key shapes
    QColor face{40, 42, 49, 235};   // keycap top face

    QColor outline{255, 255, 255, 110};
    QColor text{235, 235, 240};
    double opacity = 1.0;     // 0..1 applied to the whole image
};
// Renders the layout fitted and centered into `size` on a transparent background. Pure and
// stateless (no caching) so it is safe to call from preview, editor playback and, later, capture.
QImage renderInputOverlay(const Layout &layout, const InputState &state, const QSize &size, const RenderStyle &style = {});
} // namespace Shadowless::Input

namespace Shadowless::Input {
// Where an overlay of `layoutSize` (layout units) sits inside `video`: a corner
// ("top-left" | "top-right" | "bottom-left" | "bottom-right"), `scalePercent` of 30% of the video width,
// clamped so it never exceeds 90% of the video. Shared by the clip player and the overlay editor canvas.
// A few representative inputs held down (W, left click, a face button...) for previews and screenshots.
InputState demoState(const Layout &layout);
// Default share of the video width by layout scope: gamepads are smaller so a tall controller does not
// dwarf keyboards; the full keyboard is larger so its many keys stay legible.
double defaultWidthFraction(const QString &scope);
QRect overlayRect(const QRect &video, const QSizeF &layoutSize, const QString &corner, int scalePercent, const QString &scope = {});
} // namespace Shadowless::Input
