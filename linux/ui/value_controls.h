#pragma once
#include <QWheelEvent>

namespace Shadowless {
template <typename Control>
class ClickToAdjust : public Control {
public:
    using Control::Control;

protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};
} // namespace Shadowless
