#include "bindings.h"
#include <QJsonObject>
#include <QKeySequence>

namespace Shadowless {
QStringList legacyBindingCleanup(const QJsonArray &bindings) {
    QStringList commands;
    const QStringList ids{"toggle-overlay", "save-replay", "toggle-capture"};
    for (const auto &value : bindings) {
        const auto row = value.toObject();
        if (row.value("dispatcher").toString() != "__lua") continue;
        for (const auto &id : ids) {
            const auto description = "Captail native: " + id;
            if (row.value("description").toString() != description) continue;
            bool valid = false;
            const auto reference = row.value("arg").toString().toULongLong(&valid);
            if (!valid || !reference) continue;
            const int mask = row.value("modmask").toInt();
            const auto key = row.value("key").toString();
            HyprBinding parsed;
            if ((mask & ~77) || row.value("keycode").toInt() || row.value("catch_all").toBool() ||
                !HyprBinding::parse(key, id, &parsed) || parsed.mask || parsed.key.compare(key, Qt::CaseInsensitive)) continue;
            int matches = 0;
            for (const auto &candidate : bindings) {
                const auto other = candidate.toObject();
                if (other.value("modmask") == row.value("modmask") &&
                    other.value("key").toString().compare(key, Qt::CaseInsensitive) == 0) ++matches;
            }
            // Hyprland removes by key even through a handle; preserve shared combinations.
            if (matches != 1) continue;
            QStringList keys;
            if (mask & 1) keys << "SHIFT";
            if (mask & 4) keys << "CTRL";
            if (mask & 8) keys << "ALT";
            if (mask & 64) keys << "SUPER";
            keys << key;
            const auto slot = "__captail_native_bindings[\"" + id + "\"]";
            commands << "local b=__captail_native_bindings and " + slot +
                "; if b and b.description==\"" + description + "\" and b.arg==\"" + QString::number(reference) +
                "\" then b:remove(); " + slot + "=nil else hl.unbind(\"" + keys.join(" + ") + "\") end";
        }
    }
    return commands;
}

bool HyprBinding::parse(const QString &text, const QString &id, HyprBinding *binding) {
    const QKeySequence sequence(text, QKeySequence::PortableText);
    if (sequence.count() != 1) return false;
    const auto combination = sequence[0]; const auto key = combination.key();
    QString name;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) name = QChar('A' + key - Qt::Key_A);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9) name = QChar('0' + key - Qt::Key_0);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F35) name = "F" + QString::number(key - Qt::Key_F1 + 1);
    else if (key == Qt::Key_Space) name = "space";
    else if (key == Qt::Key_Pause) name = "Pause";
    else if (key == Qt::Key_Print) name = "Print";
    else return false;
    HyprBinding result; result.key = name; result.id = id; QStringList modifiers;
    const auto mods = combination.keyboardModifiers();
    if (mods & Qt::ShiftModifier) { modifiers << "SHIFT"; result.mask |= 1; }
    if (mods & Qt::ControlModifier) { modifiers << "CTRL"; result.mask |= 4; }
    if (mods & Qt::AltModifier) { modifiers << "ALT"; result.mask |= 8; }
    if (mods & Qt::MetaModifier) { modifiers << "SUPER"; result.mask |= 64; }
    result.modifiers = modifiers.join(' '); *binding = result; return true;
}
HyprBinding::Availability HyprBinding::availability(const QJsonArray &bindings) const {
    bool existing = false;
    for (const auto &value : bindings) {
        const auto row = value.toObject();
        if (row.value("modmask").toInt() != mask || row.value("key").toString().compare(key, Qt::CaseInsensitive) != 0) continue;
        const bool direct = row.value("dispatcher").toString() == "global" && row.value("arg").toString() == "io.github.zefth051.Shadowless:" + id;
        const auto dispatcher = row.value("dispatcher").toString();
        const bool lua = (dispatcher == "__lua" || dispatcher == "HL.Dispatcher(global)") && row.value("description").toString() == "Shadowless native: " + id;
        if (!direct && !lua) return Conflict;
        existing = true;
    }
    return existing ? Existing : Free;
}
} // namespace Shadowless
