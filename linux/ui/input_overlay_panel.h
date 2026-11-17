#pragma once
#include "../overlays/input/core/input_source.h"
#include "../overlays/input/access/input_access.h"
#include "../settings/settings.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QPushButton;
namespace Shadowless {
class InputOverlayPreview;
// "Peripheral overlay" controls for the Capture overlays editor: preview card, overlay dropdown,
// position/size/opacity, the saved-events switch and the privacy/permission notes. It owns no input:
// the controller feeds it live state and source status, and reads input only while previewWanted()
// carries a layout id.
class InputOverlayPanel : public QWidget {
    Q_OBJECT
public:
    explicit InputOverlayPanel(QWidget *parent = nullptr);
    InputOverlaySettings value() const;
    void setValue(const InputOverlaySettings &value); // does not emit changed()
    void setInputLive(const Input::InputState &state);
    void setInputStatus(Input::InputStatus status, const QString &detail);
    QString statusBadge() const; // LIVE, PERMISSION DENIED, DISCONNECTED, ERROR or IDLE; never LIVE without a readable source
signals:
    void changed(const Shadowless::InputOverlaySettings &value); // user edits only
    // Layout id while the panel is visible with an overlay selected, empty otherwise.
    void previewWanted(const QString &layoutId);
    void previewChanged(const Shadowless::InputOverlaySettings &value); // any value change, for canvas previews
    void accessChanged();                                              // input access was granted or removed: devices should be re-probed
protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    InputOverlayPreview *m_preview;
    QComboBox *m_layout, *m_corner;
    QSpinBox *m_scale, *m_opacity;
    QCheckBox *m_sidecar;
    QLabel *m_notice, *m_access, *m_setup, *m_badge, *m_placeholder, *m_accessResult;
    QPushButton *m_enableAccess, *m_removeAccess;
    Input::InputAccess *m_setupTool;
    bool m_noAgent = false;
    void enableAccess();
    void accessFinished(Input::InputAccess::Result result, const QString &message);
    Input::InputStatus m_status = Input::InputStatus::Stopped;
    QString m_statusDetail;
    bool m_loading = false;
    void refresh();
    void announce();
    void edited();
};
} // namespace Shadowless
