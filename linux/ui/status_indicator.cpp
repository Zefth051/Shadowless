#include "status_indicator.h"
#include <QPainter>
#include <QPainterPath>

namespace Shadowless {
StatusIndicator::StatusIndicator() : m_rotation(this), m_feedback(this) {
    setFixedSize(30, 30);
    setAccessibleName("Capture status");
    m_rotation.setStartValue(0.0);
    m_rotation.setEndValue(360.0);
    m_rotation.setLoopCount(-1);
    connect(&m_rotation, &QVariantAnimation::valueChanged, this, [this] { update(); });
    m_feedback.setSingleShot(true);
    m_feedback.setInterval(1600);
    connect(&m_feedback, &QTimer::timeout, this, [this] { setState(m_resume); });
    setState(Buffering);
}
void StatusIndicator::setState(State state) {
    if (state == Buffering || state == Recording) {
        m_resume = state;
        if (m_feedback.isActive()) return;
    } else m_feedback.stop();
    m_state = state;
    setAccessibleDescription(state == Buffering ? "Replay buffering" : state == Recording ? "Recording" :
                             state == Saving ? "Saving clip" : "Clip saved");
    animate();
    update();
}
void StatusIndicator::saved() {
    if (!isVisible()) return;
    setState(Saved);
    m_feedback.start();
}
void StatusIndicator::animate() {
    if (!isVisible()) { m_rotation.stop(); return; }
    const int duration = m_state == Saved ? 230 : m_state == Saving ? 1050 : 2200;
    m_rotation.setLoopCount(m_state == Saved ? 1 : -1);
    if (m_rotation.duration() != duration) { m_rotation.stop(); m_rotation.setDuration(duration); }
    if (m_rotation.state() != QAbstractAnimation::Running) m_rotation.start();
}
void StatusIndicator::showEvent(QShowEvent *) { animate(); }
void StatusIndicator::hideEvent(QHideEvent *) {
    m_feedback.stop();
    setState(m_resume);
    m_rotation.stop();
}
void StatusIndicator::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(15, 15);
    if (m_state == Saved) {
        const auto scale = 0.78 + 0.22 * m_rotation.currentValue().toReal() / 360.0;
        p.scale(scale, scale);
    }
    p.setPen(QPen(QColor("#344047"), 1));
    p.setBrush(QColor("#11171a"));
    p.drawEllipse(QPointF(0, 0), 11, 11);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#344047"), 1.5));
    p.drawEllipse(QPointF(0, 0), 9, 9);
    const auto color = m_state == Saving ? QColor("#f2c242") :
                       m_state == Recording ? QColor("#ff6666") : palette().color(QPalette::Accent);
    p.setPen(QPen(color, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (m_state == Saved) {
        p.drawEllipse(QPointF(0, 0), 9, 9);
        QPainterPath check; check.moveTo(-5.8, -0.2); check.lineTo(-2.3, 3.1); check.lineTo(5.8, -4.6);
        p.drawPath(check);
        return;
    }
    p.save();
    p.rotate(m_rotation.currentValue().toReal());
    p.drawArc(QRectF(-9, -9, 18, 18), 0, 250 * 16);
    p.restore();
    p.setPen(Qt::NoPen); p.setBrush(color);
    p.drawEllipse(QPointF(0, 0), 2, 2);
}
}
