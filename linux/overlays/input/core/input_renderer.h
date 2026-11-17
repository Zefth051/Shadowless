#pragma once
#include "input_types.h"
#include "layout.h"
#include <QColor>
#include <QImage>

namespace Shadowless::Input {
struct RenderStyle {
    QColor accent{"#8b5cf6"}; // pressed fill
    QColor idle{18, 18, 20, 200};
    QColor outline{255, 255, 255, 60};
    QColor text{235, 235, 240};
    double opacity = 1.0;     // 0..1 applied to the whole image
};
// Renders the layout fitted and centered into `size` on a transparent background. Pure and
// stateless (no caching) so it is safe to call from preview, editor playback and, later, capture.
QImage renderInputOverlay(const Layout &layout, const InputState &state, const QSize &size, const RenderStyle &style = {});
} // namespace Shadowless::Input
