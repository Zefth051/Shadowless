#include "input_renderer.h"
#include <QFont>
#include <QPainter>
#include <QPainterPath>

namespace Shadowless::Input {
namespace {
QPainterPath shapePath(const LayoutElement &e, const QRectF &r) {
    QPainterPath path;
    const double radius = qMin(r.width(), r.height());
    if (e.shape == QLatin1String("circle")) path.addEllipse(r);
    else if (e.shape == QLatin1String("mouseBody")) path.addRoundedRect(r, r.width() * .5, r.width() * .5);
    else if (e.shape == QLatin1String("padBody")) path.addRoundedRect(r, radius * .35, radius * .35);
    else if (e.shape == QLatin1String("mouseLeft") || e.shape == QLatin1String("mouseRight")) {
        // Top corner follows the body's round; inner and lower corners stay small.
        const double big = r.width() * .9, small = r.width() * .12;
        const bool left = e.shape == QLatin1String("mouseLeft");
        path.moveTo(r.left() + (left ? big : small), r.top());
        path.lineTo(r.right() - (left ? small : big), r.top());
        path.quadTo(r.right(), r.top(), r.right(), r.top() + (left ? small : big));
        path.lineTo(r.right(), r.bottom() - small);
        path.quadTo(r.right(), r.bottom(), r.right() - small, r.bottom());
        path.lineTo(r.left() + small, r.bottom());
        path.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - small);
        path.lineTo(r.left(), r.top() + (left ? big : small));
        path.quadTo(r.left(), r.top(), r.left() + (left ? big : small), r.top());
        path.closeSubpath();
    } else path.addRoundedRect(r, radius * .22, radius * .22);
    return path;
}
} // namespace

QImage renderInputOverlay(const Layout &layout, const InputState &state, const QSize &size, const RenderStyle &style) {
    QImage image(size.expandedTo({1, 1}), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!layout.isValid()) return image;
    QPainter p(&image);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setOpacity(qBound(0., style.opacity, 1.));
    const QSizeF units = layout.size();
    const double scale = qMin(image.width() / units.width(), image.height() / units.height());
    const QPointF origin((image.width() - units.width() * scale) / 2, (image.height() - units.height() * scale) / 2);
    QFont font = p.font();
    for (const LayoutElement &e : layout.elements()) {
        const QRectF r(origin.x() + e.rect.x() * scale, origin.y() + e.rect.y() * scale, e.rect.width() * scale, e.rect.height() * scale);
        const QPainterPath path = shapePath(e, r);
        const bool down = e.interactive() && state.isDown(e.id);
        p.setPen(QPen(style.outline, qMax(1., scale * .06)));
        p.setBrush(!e.interactive() ? QBrush(Qt::NoBrush) : QBrush(down ? style.accent : style.idle));
        p.drawPath(path);
        if (e.label.isEmpty()) continue;
        font.setPixelSize(qMax(6, int(qMin(r.height() * .5, r.width() * .8 / qMax<qsizetype>(1, e.label.size()) * 1.6))));
        font.setWeight(QFont::DemiBold);
        p.setFont(font);
        p.setPen(down ? QColor(255, 255, 255) : style.text);
        p.drawText(r, Qt::AlignCenter, e.label);
    }
    return image;
}

InputState demoState(const Layout &layout) {
    InputState state;
    if (!layout.isValid()) return state;
    const QSet<InputId> allowed = layout.allowlist();
    for (const char *name : {"key.w", "mouse.left", "pad.south", "pad.r1", "key.up"}) {
        const InputId id = QString::fromLatin1(name);
        if (allowed.contains(id)) state.apply({id, true, 0});
    }
    return state;
}

double defaultWidthFraction(const QString &scope) {
    return scope == QLatin1String("pad") ? .18 : scope == QLatin1String("full") ? .40 : .30;
}

QRect overlayRect(const QRect &video, const QSizeF &units, const QString &corner, int scalePercent, const QString &scope) {
    if (video.isEmpty() || units.isEmpty()) return {};
    const int margin = 14;
    int w = qBound(120, qRound(video.width() * defaultWidthFraction(scope) * scalePercent / 100.), qMax(120, int(video.width() * .9)));
    int h = qRound(w * units.height() / units.width());
    if (h > video.height() * .9) { h = int(video.height() * .9); w = qRound(h * units.width() / units.height()); }
    const bool right = corner.endsWith("right"), bottom = corner.startsWith("bottom");
    return QRect(right ? video.right() - margin - w + 1 : video.left() + margin,
                 bottom ? video.bottom() - margin - h + 1 : video.top() + margin, w, h);
}
} // namespace Shadowless::Input
