#pragma once
#include "input_types.h"
#include <QJsonObject>
#include <QRectF>
#include <QSizeF>
#include <QVector>

namespace Shadowless::Input {
struct LayoutElement {
    InputId id;      // empty for decoration (mouse/pad body)
    QString label, shape;
    QRectF rect;     // layout units
    int evdev = -1;  // Linux key/button code, -1 when not applicable (e.g. hat directions)
    int win = -1;    // Windows scan code (0xE000 | code for extended keys)
    int vk = -1;     // Windows virtual key (mouse buttons only)
    QString hat;     // gamepad hat condition, e.g. "ABS_HAT0X<0"
    bool interactive() const { return !id.isEmpty(); }
};

// A peripheral overlay description (schema 1, see docs/LINUX_INPUT_OVERLAY.md). Pure data: the
// same JSON files serve the Linux and Windows branches.
class Layout {
public:
    static constexpr int kMaxElements = 200;
    static bool fromJson(const QJsonObject &json, Layout *result, QString *error);
    static bool load(const QString &path, Layout *result, QString *error);
    // Built-in layouts live in the Qt resource ":/input-layouts/<id>.json".
    static QStringList builtinIds(); // dropdown order
    static bool builtin(const QString &id, Layout *result, QString *error);
    static QString none() { return QStringLiteral("none"); } // "no overlay" is not a Layout

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString scope() const { return m_scope; } // gameplay | full | pad
    QSizeF size() const { return m_size; }
    const QVector<LayoutElement> &elements() const { return m_elements; }
    QSet<InputId> allowlist() const; // the only ids a source may emit for this layout
    bool isValid() const { return !m_id.isEmpty(); }
    // Typing-capable layouts record text-like key sequences; the UI warns before enabling them.
    bool recordsTypingKeys() const { return m_scope == QLatin1String("full"); }
private:
    QString m_id, m_name, m_scope;
    QSizeF m_size;
    QVector<LayoutElement> m_elements;
};
} // namespace Shadowless::Input
