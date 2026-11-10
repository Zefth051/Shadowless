#include "layer.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <cmath>

namespace Shadowless {
static double finite(double value, double fallback, double low, double high) {
    return std::isfinite(value) ? qBound(low, value, high) : fallback;
}
void OverlayLayer::normalize() {
    width = finite(width, .25, .01, 1.); height = finite(height, .25, .01, 1.);
    opacity = qBound(0, opacity, 100); setPosition(x, y);
}
void OverlayLayer::setPosition(double left, double top) {
    x = finite(left, 0., 0., 1. - width); y = finite(top, 0., 0., 1. - height);
}
QRectF OverlayLayer::fitRect(QSize canvas, QSize media) const {
    if (canvas.isEmpty() || media.isEmpty()) return {};
    const QSizeF box(canvas.width() * width, canvas.height() * height);
    QSizeF fitted(media); fitted.scale(box, Qt::KeepAspectRatio);
    return QRectF(QPointF(canvas.width() * x + (box.width() - fitted.width()) / 2,
                         canvas.height() * y + (box.height() - fitted.height()) / 2), fitted);
}
bool OverlayLayer::supports(const QString &path) {
    return QStringList{"png", "jpg", "jpeg", "bmp", "webp", "gif", "mp4", "webm", "mov", "mkv"}
        .contains(QFileInfo(path).suffix().toLower());
}
QJsonObject OverlayLayer::toJson() const {
    return {{"id", id}, {"path", path}, {"enabled", enabled}, {"x", x}, {"y", y},
            {"width", width}, {"height", height}, {"opacity", opacity}};
}
QJsonObject OverlayScene::toJson() const {
    QJsonArray rows; for (const auto &layer : layers) rows.append(layer.toJson());
    return {{"version", 1}, {"enabled", enabled}, {"layers", rows}};
}
bool OverlayScene::fromJson(const QJsonObject &json, OverlayScene *result, QString *error) {
    auto fail = [error](const QString &message) { *error = message; return false; };
    if (json.value("version").toInt(-1) != 1 || !json.value("enabled").isBool()
        || !json.value("layers").isArray()) return fail("Invalid overlay scene format.");
    const auto rows = json.value("layers").toArray();
    if (rows.size() > 16) return fail("A maximum of 16 overlay layers is supported.");
    OverlayScene scene; scene.enabled = json.value("enabled").toBool(); QSet<QString> ids;
    for (const auto &value : rows) {
        if (!value.isObject()) return fail("Invalid overlay layer.");
        const auto row = value.toObject(); OverlayLayer layer;
        layer.id = row.value("id").toString(); layer.path = row.value("path").toString();
        if (layer.id.isEmpty() || layer.id.size() > 128 || ids.contains(layer.id)) return fail("Invalid or duplicate overlay ID.");
        if (layer.path.size() > 4096 || !QFileInfo(layer.path).isAbsolute() || !OverlayLayer::supports(layer.path))
            return fail("Choose a supported local image or video for each overlay.");
        if (!row.value("enabled").isBool()) return fail("Invalid layer visibility.");
        for (const auto *key : {"x", "y", "width", "height", "opacity"})
            if (!row.value(QLatin1String(key)).isDouble() || !std::isfinite(row.value(QLatin1String(key)).toDouble()))
                return fail("Invalid layer geometry or opacity.");
        layer.enabled = row.value("enabled").toBool();
        layer.x = row.value("x").toDouble(); layer.y = row.value("y").toDouble();
        layer.width = row.value("width").toDouble(); layer.height = row.value("height").toDouble();
        layer.opacity = qRound(qBound(0., row.value("opacity").toDouble(), 100.)); layer.normalize();
        ids.insert(layer.id); scene.layers.append(layer);
    }
    *result = scene; error->clear(); return true;
}
} // namespace Shadowless
