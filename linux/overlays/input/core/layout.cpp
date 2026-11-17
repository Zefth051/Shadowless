#include "layout.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>

namespace Shadowless::Input {
namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool finite(double v) { return std::isfinite(v); }
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
int code(const QJsonObject &o, const char *key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toInt(-1) : -1;
}
} // namespace

bool Layout::fromJson(const QJsonObject &json, Layout *result, QString *error) {
    if (json.value("schema").toInt() != 1) return fail(error, "unsupported layout schema");
    static const QRegularExpression idPattern("^[a-z0-9]+(-[a-z0-9]+)*$");
    static const QRegularExpression inputPattern("^(key|mouse|pad)\\.[a-z0-9]+(\\.[a-z0-9]+)*$");
    Layout layout;
    layout.m_id = text(json, "id"); layout.m_name = text(json, "name"); layout.m_scope = text(json, "scope");
    if (!idPattern.match(layout.m_id).hasMatch() || layout.m_id == none()) return fail(error, "invalid layout id");
    if (layout.m_name.isEmpty() || layout.m_name.size() > 80) return fail(error, "invalid layout name");
    if (!QStringList{"gameplay", "full", "pad"}.contains(layout.m_scope)) return fail(error, "invalid layout scope");
    const double width = json.value("width").toDouble(), height = json.value("height").toDouble();
    if (!finite(width) || !finite(height) || width < 1 || height < 1 || width > 100 || height > 100)
        return fail(error, "invalid layout size");
    layout.m_size = {width, height};
    const QJsonArray elements = json.value("elements").toArray();
    if (elements.isEmpty() || elements.size() > kMaxElements) return fail(error, "invalid element count");
    QSet<InputId> seen;
    for (const QJsonValue &value : elements) {
        const QJsonObject o = value.toObject();
        LayoutElement e;
        e.id = text(o, "id"); e.label = text(o, "label"); e.shape = text(o, "shape");
        e.rect = QRectF(o.value("x").toDouble(-1), o.value("y").toDouble(-1), o.value("w").toDouble(), o.value("h").toDouble());
        e.evdev = code(o, "evdev"); e.win = code(o, "win"); e.vk = code(o, "vk"); e.hat = text(o, "hat");
        if (!QStringList{"round", "circle", "mouseBody", "mouseLeft", "mouseRight", "padBody"}.contains(e.shape))
            return fail(error, "unknown element shape");
        if (!finite(e.rect.x()) || !finite(e.rect.y()) || !finite(e.rect.width()) || !finite(e.rect.height())
            || e.rect.x() < 0 || e.rect.y() < 0 || e.rect.width() <= 0 || e.rect.height() <= 0
            || e.rect.right() > width + 1e-6 || e.rect.bottom() > height + 1e-6)
            return fail(error, "element outside the layout bounds");
        if (e.label.size() > 8) return fail(error, "label too long");
        if (e.interactive()) {
            if (!inputPattern.match(e.id).hasMatch()) return fail(error, "invalid input id: " + e.id);
            if (seen.contains(e.id)) return fail(error, "duplicate input id: " + e.id);
            seen.insert(e.id);
        }
        layout.m_elements.append(e);
    }
    if (seen.isEmpty()) return fail(error, "layout has no inputs");
    *result = layout;
    return true;
}

bool Layout::load(const QString &path, Layout *result, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, "cannot open layout " + path);
    if (file.size() > 256 * 1024) return fail(error, "layout file too large");
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) return fail(error, "layout is not valid JSON");
    return fromJson(doc.object(), result, error);
}

QStringList Layout::builtinIds() { return builtinLayoutIds(); }

bool Layout::builtin(const QString &id, Layout *result, QString *error) {
    if (!builtinIds().contains(id)) return fail(error, "unknown built-in layout");
    return load(QStringLiteral(":/input-layouts/%1.json").arg(id), result, error);
}

QSet<InputId> Layout::allowlist() const {
    QSet<InputId> ids;
    for (const LayoutElement &e : m_elements) if (e.interactive()) ids.insert(e.id);
    return ids;
}
} // namespace Shadowless::Input
