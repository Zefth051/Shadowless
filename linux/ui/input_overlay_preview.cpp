#include "input_overlay_preview.h"
#include <QPainter>

namespace Shadowless {
InputOverlayPreview::InputOverlayPreview(QWidget *parent) : QWidget(parent) {
    setObjectName("inputOverlayPreview");
    setMinimumHeight(120);
    setAccessibleName("Peripheral overlay preview");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(190);
}
void InputOverlayPreview::setLayoutId(const QString &id) {
    m_id = id;
    m_layout = Input::Layout();
    QString error;
    if (id != Input::Layout::none()) Input::Layout::builtin(id, &m_layout, &error);
    rebuildDemo();
    setAccessibleDescription(m_layout.isValid() ? m_layout.name() : "No peripheral overlay");
    update();
}
void InputOverlayPreview::setOpacity(int percent) { m_style.opacity = qBound(20, percent, 100) / 100.; update(); }
void InputOverlayPreview::setLiveState(const Input::InputState &state) { m_live = state; m_isLive = true; update(); }
void InputOverlayPreview::clearLiveState() { m_isLive = false; m_live.clear(); update(); }
void InputOverlayPreview::rebuildDemo() {
    m_demo.clear();
    if (!m_layout.isValid()) return;
    const auto allowed = m_layout.allowlist();
    for (const char *name : {"key.w", "mouse.left", "pad.south", "pad.r1", "key.up"}) {
        const Input::InputId id = QString::fromLatin1(name);
        if (allowed.contains(id)) m_demo.apply({id, true, 0});
    }
}
void InputOverlayPreview::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
    p.setPen(QPen(QColor("#283136"), 1));
    p.setBrush(QColor("#0f1315"));
    p.drawRoundedRect(card, 14, 14);
    if (!m_layout.isValid()) {
        p.setPen(QColor("#8f9ba2"));
        p.drawText(card, Qt::AlignCenter, "No peripheral overlay selected");
        return;
    }
    const qreal ratio = devicePixelRatioF();
    const QRect inner = rect().adjusted(18, 16, -18, -16);
    Input::RenderStyle style = m_style;
    style.accent = window()->palette().color(QPalette::Accent); // follows the application accent
    QImage image = Input::renderInputOverlay(m_layout, m_isLive ? m_live : m_demo, inner.size() * ratio, style);
    image.setDevicePixelRatio(ratio);
    p.drawImage(inner.topLeft(), image);
}
} // namespace Shadowless
