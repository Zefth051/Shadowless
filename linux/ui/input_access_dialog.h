#pragma once
#include "../overlays/input/access/input_access.h"
#include <QFrame>
#include <QVector>

class QCheckBox;
class QComboBox;
class QPushButton;
namespace Shadowless {
// Inline consent card (not a separate window, so it can never end up behind the overlay editor on layer-shell
// compositors). Lists one row per detected physical device, preselected and classified automatically; nothing is
// changed until the system authorization dialog that follows "Allow access".
class InputAccessDialog : public QFrame {
    Q_OBJECT
public:
    explicit InputAccessDialog(const QVector<Input::AccessDevice> &groups, QWidget *parent = nullptr);
    QVector<Input::AccessDevice> selected() const;
signals:
    void accepted();
    void rejected();
private:
    struct Row { Input::AccessDevice group; QCheckBox *box; QComboBox *kind; };
    QVector<Row> m_rows;
    QPushButton *m_allow = nullptr;
    void updateButtons();
};
} // namespace Shadowless
