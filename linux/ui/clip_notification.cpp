#include "clip_notification.h"
#include "../platform/hyprland/overlay_surface.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QPainter>
#define LAYERSHELLQTINTERFACE_DISABLE_DEPRECATED_BEFORE_AND_AT 0x060600
#include <LayerShellQt/Window>
namespace Shadowless {
ClipNotification::ClipNotification() {
    setObjectName("clipNotification");
    setFixedSize(360, 86);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(65, 14, 16, 14);
    layout->setSpacing(4);
    m_title = new QLabel;
    m_detail = new QLabel;
    for (auto *label : {m_title, m_detail}) { label->setTextFormat(Qt::PlainText); layout->addWidget(label); }
    m_title->setStyleSheet("color: #f1f5f4; font-size: 14px; font-weight: bold;");
    m_detail->setStyleSheet("color: #8f9ba2; font-size: 12px;");
    for (auto *label : {m_title, m_detail}) label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    OverlaySurface::prepare(this, true);
    auto *surface = LayerShellQt::Window::get(windowHandle());
    surface->setScope("shadowless-notification");
    surface->setMargins(QMargins(0, 58, 18, 0));
    m_lifetime.setStartValue(1.0);
    m_lifetime.setEndValue(0.0);
    connect(&m_lifetime, &QVariantAnimation::valueChanged, this, [this] { update(); });
    connect(&m_lifetime, &QVariantAnimation::finished, this, &QWidget::hide);
}
void ClipNotification::present(const QString &title, const QString &detail, int timeoutMs, Kind kind) {
    m_kind = kind;
    setAccessibleName(title);
    setAccessibleDescription(detail);
    m_title->setText(m_title->fontMetrics().elidedText(title, Qt::ElideRight, 279));
    m_detail->setText(m_detail->fontMetrics().elidedText(detail, Qt::ElideMiddle, 279));
    m_detail->setVisible(!detail.isEmpty());
    m_lifetime.stop();
    m_lifetime.setDuration(qMax(1, timeoutMs));
    m_lifetime.start();
    OverlaySurface::show(this);
}
void ClipNotification::dismiss() { hide(); }
void ClipNotification::hideEvent(QHideEvent *event) {
    m_lifetime.stop();
    QWidget::hideEvent(event);
}
void ClipNotification::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#283136"), 1));
    painter.setBrush(QColor("#161b1e"));
    painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 14, 14);
    const QColor color = m_kind == Failed ? QColor("#ff6666") : m_accent;
    painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(17, 26, 34, 34));
    if (m_kind == Saved) {
        painter.drawLine(QPointF(26, 43), QPointF(32, 49));
        painter.drawLine(QPointF(32, 49), QPointF(43, 37));
    } else if (m_kind == Saving) {
        painter.drawLine(QPointF(34, 33), QPointF(34, 43));
        painter.drawLine(QPointF(34, 43), QPointF(41, 46));
    } else {
        painter.drawLine(QPointF(34, 34), QPointF(34, 45));
        painter.drawPoint(QPointF(34, 51));
    }
    const qreal remaining = m_lifetime.currentValue().toReal();
    if (remaining > 0)
        painter.drawLine(QPointF(15, height() - 2), QPointF(15 + (width() - 30) * remaining, height() - 2));
}
}
