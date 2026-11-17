#include "input_timeline.h"
#include "layout.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <algorithm>

namespace Shadowless::Input {
namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
} // namespace

bool InputTimeline::load(const QString &file, InputTimeline *result, QString *error) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return fail(error, "no input events for this clip");
    if (f.size() > kMaxFileBytes) return fail(error, "input events file too large");
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) return fail(error, "input events file is not valid JSON");
    return fromJson(doc.object(), result, error);
}

bool InputTimeline::fromJson(const QJsonObject &json, InputTimeline *result, QString *error) {
    if (json.value("schema").toInt() != 1) return fail(error, "unsupported input events schema");
    static const QRegularExpression layoutPattern("^[a-z0-9]+(-[a-z0-9]+)*$");
    static const QRegularExpression idPattern("^(key|mouse|pad)\\.[a-z0-9]+(\\.[a-z0-9]+)*$");
    InputTimeline t;
    t.m_layout = json.value("layout").toString();
    if (!layoutPattern.match(t.m_layout).hasMatch()) return fail(error, "invalid layout in input events");
    t.m_durationMs = qMax<qint64>(0, qint64(json.value("durationMs").toDouble()));
    t.m_truncated = json.value("truncated").toBool();
    // Only built-in layouts have a known allowlist; for others nothing can be verified, so refuse.
    Layout layout; QString layoutError;
    if (!Layout::builtin(t.m_layout, &layout, &layoutError)) return fail(error, "unknown layout in input events");
    const QSet<InputId> allowed = layout.allowlist();
    const QJsonArray events = json.value("events").toArray();
    if (events.size() > kMaxEvents) return fail(error, "too many input events");
    t.m_events.reserve(events.size());
    for (const QJsonValue &value : events) {
        const QJsonArray e = value.toArray();
        if (e.size() != 3 || !e.at(0).isDouble() || !e.at(1).isString() || !e.at(2).isDouble())
            return fail(error, "malformed input event");
        const double ms = e.at(0).toDouble();
        const int down = e.at(2).toInt(-1);
        const QString id = e.at(1).toString();
        if (!(ms >= 0 && ms < 1e11) || (down != 0 && down != 1) || !idPattern.match(id).hasMatch())
            return fail(error, "malformed input event");
        if (!allowed.contains(id)) continue; // not part of the recorded layout
        t.m_events.append({id, down == 1, qint64(ms)});
    }
    std::stable_sort(t.m_events.begin(), t.m_events.end(), [](const InputEvent &a, const InputEvent &b) { return a.tMs < b.tMs; });
    *result = t;
    return true;
}

InputState InputTimeline::stateAt(qint64 ms) const {
    if (ms < m_cachedMs) { m_cursor = 0; m_cached.clear(); }
    while (m_cursor < m_events.size() && m_events.at(m_cursor).tMs <= ms) m_cached.apply(m_events.at(m_cursor++));
    m_cachedMs = ms;
    return m_cached;
}
} // namespace Shadowless::Input
