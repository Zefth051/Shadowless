#include "input_overlay_layer.h"
#include <QPainter>

namespace Shadowless {
InputOverlayLayer::InputOverlayLayer(QWidget *parent) : QWidget(parent) {
    setObjectName("inputOverlayLayer");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAccessibleName("Input overlay");
    hide();
}
bool InputOverlayLayer::load(const QString &clipPath) {
    clear();
    Input::InputTimeline timeline; Input::Layout layout; QString error;
    if (!Input::InputTimeline::load(Input::InputTimeline::sidecarPath(clipPath), &timeline, &error)) return false;
    if (!Input::Layout::builtin(timeline.layoutId(), &layout, &error)) return false;
    m_timeline = timeline; m_layout = layout;
    setAccessibleDescription(layout.name());
    place(m_video);
    return true;
}
void InputOverlayLayer::clear() {
    m_timeline = Input::InputTimeline(); m_layout = Input::Layout(); m_state.clear();
    hide();
}
void InputOverlayLayer::setSettings(const InputOverlaySettings &settings) { m_settings = settings.sanitized(); place(m_video); update(); }
void InputOverlayLayer::setPositionMs(qint64 ms) {
    if (!hasTimeline()) return;
    const Input::InputState next = m_timeline.stateAt(ms);
    if (next == m_state) return;
    m_state = next;
    update();
}
void InputOverlayLayer::place(const QRect &video) {
    m_video = video;
    if (!hasTimeline() || video.isEmpty()) return;
    const QSizeF units = m_layout.size();
    const int margin = 14;
    int w = qBound(120, qRound(video.width() * .30 * m_settings.scale / 100.), qMax(120, int(video.width() * .9)));
    int h = qRound(w * units.height() / units.width());
    if (h > video.height() * .9) { h = int(video.height() * .9); w = qRound(h * units.width() / units.height()); }
    const bool right = m_settings.corner.endsWith("right"), bottom = m_settings.corner.startsWith("bottom");
    setGeometry(right ? video.right() - margin - w + 1 : video.left() + margin,
                bottom ? video.bottom() - margin - h + 1 : video.top() + margin, w, h);
}
void InputOverlayLayer::paintEvent(QPaintEvent *) {
    if (!hasTimeline()) return;
    QPainter p(this);
    const qreal ratio = devicePixelRatioF();
    Input::RenderStyle style;
    style.accent = window()->palette().color(QPalette::Accent);
    style.opacity = m_settings.opacity / 100.;
    QImage image = Input::renderInputOverlay(m_layout, m_state, size() * ratio, style);
    image.setDevicePixelRatio(ratio);
    p.drawImage(QPoint(0, 0), image);
}
} // namespace Shadowless
