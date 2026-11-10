#include "bindings.h"
#include <QJsonObject>
#include <QKeySequence>

namespace Shadowless {
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
        const bool lua = row.value("dispatcher").toString() == "__lua" && row.value("description").toString() == "Shadowless native: " + id;
        if (!direct && !lua) return Conflict;
        existing = true;
    }
    return existing ? Existing : Free;
}
} // namespace Shadowless
