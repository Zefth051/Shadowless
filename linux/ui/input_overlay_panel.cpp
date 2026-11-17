#include "input_overlay_panel.h"
#include "input_overlay_preview.h"
#include "input_access_dialog.h"
#include "value_controls.h"
#include "../overlays/input/core/layout.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QShowEvent>
#include <QSpinBox>
#include <QVBoxLayout>
#include <unistd.h>

namespace Shadowless {
namespace {
QLabel *note(const QString &text) {
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setProperty("muted", true);
    return label;
}
} // namespace

InputOverlayPanel::InputOverlayPanel(QWidget *parent) : QWidget(parent) {
    setObjectName("inputOverlayPanel");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel("PERIPHERAL OVERLAY");
    title->setObjectName("sectionTitle");
    layout->addWidget(title);
    layout->addWidget(note("Show pressed keys, mouse buttons or gamepad inputs in the video."));
    m_preview = new InputOverlayPreview;
    layout->addWidget(m_preview);
    m_badge = new QLabel; m_badge->setObjectName("inputOverlayStatus"); m_badge->setAlignment(Qt::AlignCenter); m_badge->setTextFormat(Qt::PlainText);
    layout->addWidget(m_badge);
    m_layout = new ClickToAdjust<QComboBox>;
    m_layout->setObjectName("inputOverlayLayout");
    m_layout->setAccessibleName("Peripheral overlay");
    m_layout->addItem("None", "none");
    for (const QString &id : Input::Layout::builtinIds()) {
        Input::Layout l; QString error;
        m_layout->addItem(Input::Layout::builtin(id, &l, &error) ? l.name() : id, id);
    }
    layout->addWidget(m_layout);
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 4, 0, 0);
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_corner = new ClickToAdjust<QComboBox>;
    m_corner->setObjectName("inputOverlayCorner");
    m_corner->addItem("Left down", "bottom-left");
    m_corner->addItem("Right down", "bottom-right");
    m_corner->addItem("Left up", "top-left");
    m_corner->addItem("Right up", "top-right");
    form->addRow("Position", m_corner);
    m_scale = new ClickToAdjust<QSpinBox>;
    m_scale->setObjectName("inputOverlayScale");
    m_scale->setRange(50, 200); m_scale->setSingleStep(10); m_scale->setSuffix(" %"); m_scale->setValue(100);
    form->addRow("Size", m_scale);
    m_opacity = new ClickToAdjust<QSpinBox>;
    m_opacity->setObjectName("inputOverlayOpacity");
    m_opacity->setRange(20, 100); m_opacity->setSingleStep(10); m_opacity->setSuffix(" %"); m_opacity->setValue(100);
    form->addRow("Opacity", m_opacity);
    m_sidecar = new QCheckBox("Save input events next to each clip");
    m_sidecar->setObjectName("inputOverlaySidecar");
    m_sidecar->setToolTip("Lets the clip player show this overlay over clean video. Only the keys in the selected overlay are stored.");
    form->addRow(m_sidecar);
    layout->addLayout(form);
    m_notice = note({}); m_notice->setObjectName("inputOverlayNotice");
    m_access = note({}); m_access->setObjectName("inputOverlayAccess");
    m_setup = note(
        "Shadowless never changes permissions and does not need the \"input\" group. To allow reading only the device classes you need, "
        "create /etc/udev/rules.d/70-shadowless-input.rules with one line per class, for example:\n"
        "SUBSYSTEM==\"input\", KERNEL==\"event*\", ENV{ID_INPUT_MOUSE}==\"1\", TAG+=\"uaccess\"\n"
        "(ID_INPUT_KEYBOARD and ID_INPUT_JOYSTICK work the same way), then run: sudo udevadm control --reload && sudo udevadm trigger -s input. "
        "Details: docs/LINUX_INPUT_OVERLAY.md.");
    m_setup->setObjectName("inputOverlaySetup");
    m_setup->setTextFormat(Qt::PlainText);
    m_setup->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_placeholder = note("Temporary placeholder layout: the controller artwork will be replaced by a proper DualSense / Xbox model.");
    m_placeholder->setObjectName("inputOverlayPlaceholder");
    m_setupTool = new Input::InputAccess(this);
    m_enableAccess = new QPushButton("Enable input access…");
    m_enableAccess->setObjectName("inputOverlayEnableAccess");
    m_enableAccess->setToolTip("Asks your system for permission to read the keyboard and mouse you choose. No terminal needed.");
    m_removeAccess = new QPushButton("Remove input access");
    m_removeAccess->setObjectName("inputOverlayRemoveAccess");
    m_accessResult = note({}); m_accessResult->setObjectName("inputOverlayAccessResult"); m_accessResult->hide();
    for (auto *label : {m_notice, m_access, m_accessResult}) layout->addWidget(label);
    layout->addWidget(m_enableAccess); layout->addWidget(m_removeAccess);
    layout->addWidget(m_setup); layout->addWidget(m_placeholder);
    connect(m_enableAccess, &QPushButton::clicked, this, [this] { enableAccess(); });
    connect(m_removeAccess, &QPushButton::clicked, this, [this] { m_accessResult->hide(); m_enableAccess->setEnabled(false); m_removeAccess->setEnabled(false); m_setupTool->revoke(); });
    connect(m_setupTool, &Input::InputAccess::finished, this, [this](Input::InputAccess::Result r, const QString &m) { accessFinished(r, m); });
    connect(m_layout, &QComboBox::currentIndexChanged, this, [this] { edited(); });
    connect(m_corner, &QComboBox::currentIndexChanged, this, [this] { edited(); });
    connect(m_scale, &QSpinBox::valueChanged, this, [this] { edited(); });
    connect(m_opacity, &QSpinBox::valueChanged, this, [this] { edited(); });
    connect(m_sidecar, &QCheckBox::toggled, this, [this] { edited(); });
    refresh();
}

InputOverlaySettings InputOverlayPanel::value() const {
    InputOverlaySettings v;
    v.layout = m_layout->currentData().toString();
    v.sidecar = m_sidecar->isChecked();
    v.corner = m_corner->currentData().toString();
    v.scale = m_scale->value(); v.opacity = m_opacity->value();
    return v.sanitized();
}
void InputOverlayPanel::setValue(const InputOverlaySettings &value) {
    const auto v = value.sanitized();
    m_loading = true;
    m_layout->setCurrentIndex(qMax(0, m_layout->findData(v.layout)));
    m_corner->setCurrentIndex(qMax(0, m_corner->findData(v.corner)));
    m_scale->setValue(v.scale); m_opacity->setValue(v.opacity);
    m_sidecar->setChecked(v.sidecar);
    m_loading = false;
    refresh();
    emit previewChanged(this->value());
}
void InputOverlayPanel::edited() {
    refresh();
    if (m_loading) return;
    emit previewChanged(value());
    emit changed(value());
}
// Reflects the selected layout: preview, which controls apply, privacy wording and device access.
void InputOverlayPanel::refresh() {
    const QString id = m_layout->currentData().toString();
    const bool enabled = id != Input::Layout::none();
    m_preview->setLayoutId(id);
    m_preview->setOpacity(m_opacity->value());
    for (QWidget *w : QList<QWidget *>{m_corner, m_scale, m_opacity, m_sidecar}) w->setEnabled(enabled);
    if (!enabled && m_sidecar->isChecked()) { const QSignalBlocker block(m_sidecar); m_sidecar->setChecked(false); }
    Input::Layout layout; QString error;
    const bool typing = enabled && Input::Layout::builtin(id, &layout, &error) && layout.recordsTypingKeys();
    m_notice->setText(!enabled ? "Off. No keyboard, mouse or gamepad input is read until you choose an overlay."
        : typing ? "This layout includes every letter key. The timing of key presses could be used to reconstruct typed text, "
                   "so enable the saved events only if you accept that. Prefer a gameplay layout."
                 : "Only the keys and buttons shown above are read. Nothing is logged or stored unless saved events are on, "
                   "and then only those keys with their timing, never text.");
    // Prefer the real source status; before the source runs fall back to a read-permission probe (no device is opened).
    bool readable = false, present = false;
    for (const QString &name : QDir("/dev/input").entryList({"event*"}, QDir::System | QDir::Files | QDir::NoDotAndDotDot)) {
        present = true;
        if (::access(QFile::encodeName("/dev/input/" + name).constData(), R_OK) == 0) { readable = true; break; }
    }
    using Input::InputStatus;
    const bool denied = m_status == InputStatus::PermissionDenied || (m_status == InputStatus::Stopped && present && !readable);
    const bool empty = m_status == InputStatus::NoDevices || !present;
    const bool failed = m_status == InputStatus::Error;
    m_access->setVisible(enabled && (denied || empty || failed));
    const bool automatic = m_setupTool->available();
    m_enableAccess->setVisible(enabled && denied && automatic && !m_consent);
    m_enableAccess->setEnabled(!m_setupTool->busy());
    m_removeAccess->setVisible(enabled && automatic && m_setupTool->ruleInstalled());
    m_removeAccess->setEnabled(!m_setupTool->busy());
    m_setup->setVisible(enabled && denied && (!automatic || m_noAgent));   // manual steps only when automatic setup cannot work (no pkexec / helper)
    m_access->setText(denied ? (automatic ? QString("Shadowless cannot read your input devices yet, so the overlay stays empty. Use the button below to allow it; you can skip this and keep using Shadowless without input capture.")
                                          : QString("Shadowless cannot read your input devices yet, so the overlay will stay empty. Setup steps are below."))
                      : failed ? (m_statusDetail.isEmpty() ? QString("Input devices could not be read.") : m_statusDetail)
                               : "No keyboard, mouse or gamepad was found.");
    m_placeholder->setVisible(enabled && layout.isValid() && layout.scope() == "pad");
    const QString badge = statusBadge();
    m_badge->setVisible(enabled);
    m_badge->setText(badge);
    m_badge->setProperty("inputState", badge.toLower().replace(' ', '-'));
    m_badge->setToolTip(badge == "LIVE" ? "Showing real input." : "The preview shows the layout only; no key presses are simulated.");
    announce();
}
QString InputOverlayPanel::statusBadge() const {
    using Input::InputStatus;
    if (m_layout->currentData().toString() == Input::Layout::none()) return "OFF";
    switch (m_status) {
    case InputStatus::Active: return "LIVE";
    case InputStatus::PermissionDenied: return "PERMISSION DENIED";
    case InputStatus::NoDevices: return "DISCONNECTED";
    case InputStatus::Error: return "ERROR";
    default: break;
    }
    // Not started yet: tell a missing permission apart without opening any device.
    bool present = false, readable = false;
    for (const QString &name : QDir("/dev/input").entryList({"event*"}, QDir::System | QDir::Files | QDir::NoDotAndDotDot)) {
        present = true;
        if (::access(QFile::encodeName("/dev/input/" + name).constData(), R_OK) == 0) { readable = true; break; }
    }
    return !present ? "DISCONNECTED" : !readable ? "PERMISSION DENIED" : "IDLE";
}
void InputOverlayPanel::enableAccess() {
    m_accessResult->hide();
    if (m_consent) return;
    const auto groups = Input::InputAccess::groupDevices(Input::InputAccess::enumerate());
    bool missing = false;
    for (const auto &g : groups) missing = missing || !g.readable;
    if (groups.isEmpty()) { m_accessResult->setText("No keyboard, mouse or controller was found."); m_accessResult->show(); return; }
    if (!missing) { m_accessResult->setText("Your input devices are already readable."); m_accessResult->show(); emit accessChanged(); return; }
    // Inline card in the panel: a separate window would open behind the layer-shell editor.
    m_consent = new InputAccessDialog(groups, this);
    static_cast<QVBoxLayout *>(layout())->insertWidget(layout()->indexOf(m_enableAccess), m_consent);
    m_enableAccess->hide();
    auto close = [this] { if (m_consent) { m_consent->hide(); m_consent->deleteLater(); m_consent = nullptr; } refresh(); };
    connect(m_consent, &InputAccessDialog::rejected, this, close);
    connect(m_consent, &InputAccessDialog::accepted, this, [this, close] {
        const auto chosen = m_consent->selected();
        close();
        if (chosen.isEmpty()) return;
        m_enableAccess->setEnabled(false); m_removeAccess->setEnabled(false);
        m_setupTool->grant(chosen);
    });
}
void InputOverlayPanel::accessFinished(Input::InputAccess::Result result, const QString &message) {
    m_accessResult->setText(message); m_accessResult->setVisible(!message.isEmpty());
    if (result == Input::InputAccess::Result::NoAgent) m_noAgent = true;   // the dialog cannot appear here: show the manual steps
    refresh();
    if (result == Input::InputAccess::Result::Cancelled || result == Input::InputAccess::Result::Unavailable) return;
    emit accessChanged();                                  // udev applies the new ACLs asynchronously: probe again shortly after as well
    QTimer::singleShot(1200, this, [this] { emit accessChanged(); });
}
void InputOverlayPanel::announce() {
    const QString id = m_layout->currentData().toString();
    emit previewWanted(isVisible() && id != Input::Layout::none() ? id : QString());
}
void InputOverlayPanel::setInputLive(const Input::InputState &state) { m_preview->setLiveState(state); }
void InputOverlayPanel::setInputStatus(Input::InputStatus status, const QString &detail) {
    m_status = status; m_statusDetail = detail;
    if (status != Input::InputStatus::Active) m_preview->clearLiveState();
    refresh();
}
void InputOverlayPanel::showEvent(QShowEvent *event) { QWidget::showEvent(event); announce(); }
void InputOverlayPanel::hideEvent(QHideEvent *event) { QWidget::hideEvent(event); emit previewWanted(QString()); }
} // namespace Shadowless
