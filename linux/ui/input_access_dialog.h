#pragma once
#include "../overlays/input/access/input_access.h"
#include <QDialog>
#include <QVector>

class QCheckBox;
class QDialogButtonBox;
namespace Shadowless {
// Plain-language consent step: shows the detected keyboards, mice and controllers and lets the user choose
// which ones Shadowless may read. Nothing is changed until the system authorization dialog that follows.
class InputAccessDialog : public QDialog {
    Q_OBJECT
public:
    InputAccessDialog(const QVector<Input::AccessDevice> &devices, QWidget *parent = nullptr);
    QVector<Input::AccessDevice> selected() const;
private:
    struct Row { Input::AccessDevice device; QCheckBox *box; };
    QVector<Row> m_rows;
    QDialogButtonBox *m_buttons;
    void updateButtons();
};
} // namespace Shadowless
