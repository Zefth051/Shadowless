#pragma once
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

namespace Shadowless {
class StatusIndicator : public QWidget {
public:
    enum State { Buffering, Recording, Saving, Saved };
    StatusIndicator();
    void setState(State state);
    void saved();
protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    State m_state = Buffering, m_resume = Buffering;
    QVariantAnimation m_rotation;
    QTimer m_feedback;
    void animate();
};
}
