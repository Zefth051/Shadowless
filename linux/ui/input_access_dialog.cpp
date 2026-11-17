#include "input_access_dialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

namespace Shadowless {
using Input::AccessDevice;
namespace {
QString kind(AccessDevice::Class cls) {
    return cls == AccessDevice::Keyboard ? "keyboard keys" : cls == AccessDevice::Mouse ? "mouse buttons" : "controller buttons";
}
} // namespace

InputAccessDialog::InputAccessDialog(const QVector<AccessDevice> &devices, QWidget *parent) : QDialog(parent) {
    setObjectName("inputAccessDialog");
    setWindowTitle("Enable input access");
    setModal(true);
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(
        "To show your key presses, Shadowless needs permission to read the devices below. Your system will ask for your password once; "
        "Shadowless never sees it, and the rest of the app does not run with extra rights.\n\n"
        "Only the devices you tick get access, for your logged-in desktop session. It stays after restarts and reconnects, "
        "and you can remove it here at any time. While it is enabled, other programs you run can also read those devices.");
    intro->setWordWrap(true); intro->setTextFormat(Qt::PlainText);
    layout->addWidget(intro);
    // Preselect only the main node of each physical device (the usual keyboard or mouse); extra interfaces stay optional.
    QSet<QString> primary;
    for (const auto &d : Input::InputAccess::primaryNodes(devices)) primary.insert(d.triple());
    QSet<QString> shown;
    for (const auto &d : devices) {
        if (shown.contains(d.triple())) continue;
        shown.insert(d.triple());
        auto *box = new QCheckBox(QString("%1: %2%3").arg(d.name.isEmpty() ? QString("Unnamed device") : d.name, kind(d.cls),
                                                          d.readable ? " (already accessible)" : QString()));
        box->setObjectName("inputAccessDevice");
        box->setProperty("triple", d.triple());
        box->setChecked(!d.readable && primary.contains(d.triple()));
        box->setEnabled(!d.readable);
        connect(box, &QCheckBox::toggled, this, [this] { updateButtons(); });
        layout->addWidget(box);
        m_rows.append({d, box});
    }
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *ok = m_buttons->addButton("Continue", QDialogButtonBox::AcceptRole);
    ok->setObjectName("inputAccessContinue");
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttons);
    updateButtons();
}

QVector<AccessDevice> InputAccessDialog::selected() const {
    QVector<AccessDevice> chosen;
    for (const auto &row : m_rows) if (row.box->isChecked() && row.box->isEnabled()) chosen.append(row.device);
    return chosen;
}

void InputAccessDialog::updateButtons() {
    m_buttons->button(QDialogButtonBox::Cancel)->setText("Not now");
    for (auto *b : m_buttons->buttons()) if (m_buttons->buttonRole(b) == QDialogButtonBox::AcceptRole) b->setEnabled(!selected().isEmpty());
}
} // namespace Shadowless
