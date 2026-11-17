#include "input_access_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Shadowless {
using Input::AccessDevice;
namespace {
QString kindName(AccessDevice::Class cls) {
    return cls == AccessDevice::Keyboard ? "Keyboard" : cls == AccessDevice::Mouse ? "Mouse" : "Controller";
}
} // namespace

InputAccessDialog::InputAccessDialog(const QVector<AccessDevice> &groups, QWidget *parent) : QFrame(parent) {
    setObjectName("inputAccessDialog");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(6);
    auto *intro = new QLabel("Allow Shadowless to read these devices to show your key presses. Your system asks for your password once; "
                             "Shadowless never sees it. The access stays after restarts and you can remove it here. While it is on, "
                             "your other programs can read these devices too.");
    intro->setWordWrap(true); intro->setTextFormat(Qt::PlainText);
    layout->addWidget(intro);
    for (const auto &group : groups) {
        auto *line = new QHBoxLayout;
        auto *box = new QCheckBox(group.name.isEmpty() ? QString("Unnamed device") : group.name);
        box->setObjectName("inputAccessDevice");
        box->setProperty("triple", group.triple());
        box->setChecked(!group.readable);
        box->setEnabled(!group.readable);
        connect(box, &QCheckBox::toggled, this, [this] { updateButtons(); });
        line->addWidget(box, 1);
        QComboBox *kind = nullptr;
        if (group.alternatives.isEmpty() || group.readable) {
            auto *label = new QLabel(group.readable ? kindName(group.cls) + " · ready" : kindName(group.cls));
            label->setObjectName("inputAccessKind");
            line->addWidget(label);
        } else {
            // Hybrid device (e.g. a keyboard with a built-in pointer): the automatic choice is preselected, the other is one click away.
            kind = new QComboBox;
            kind->setObjectName("inputAccessKind");
            kind->addItem(kindName(group.cls), int(group.cls));
            for (auto other : group.alternatives) kind->addItem(kindName(other), int(other));
            line->addWidget(kind);
        }
        layout->addLayout(line);
        m_rows.append({group, box, kind});
    }
    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *later = new QPushButton("Not now");
    later->setObjectName("inputAccessCancel");
    m_allow = new QPushButton("Allow access");
    m_allow->setObjectName("inputAccessContinue");
    m_allow->setDefault(true);
    buttons->addWidget(later); buttons->addWidget(m_allow);
    layout->addLayout(buttons);
    connect(later, &QPushButton::clicked, this, &InputAccessDialog::rejected);
    connect(m_allow, &QPushButton::clicked, this, &InputAccessDialog::accepted);
    updateButtons();
}

QVector<AccessDevice> InputAccessDialog::selected() const {
    QVector<AccessDevice> chosen;
    for (const auto &row : m_rows) {
        if (!row.box->isChecked() || !row.box->isEnabled()) continue;
        chosen.append(row.kind ? row.group.asClass(AccessDevice::Class(row.kind->currentData().toInt())) : row.group);
    }
    return chosen;
}

void InputAccessDialog::updateButtons() { m_allow->setEnabled(!selected().isEmpty()); }
} // namespace Shadowless
