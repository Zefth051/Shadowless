#pragma once
#include <QWidget>

namespace Shadowless {
class OverlaySurface {
public:
    static void prepare(QWidget *widget, bool passive = false);
    static void show(QWidget *widget);
};
} // namespace Shadowless
