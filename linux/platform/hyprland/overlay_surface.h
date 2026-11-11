#pragma once
#include <QWidget>

namespace Shadowless {
class OverlaySurface {
public:
    static void prepare(QWidget *widget, bool passive = false);
    static void setPosition(QWidget *widget, const QString &position);
    static void show(QWidget *widget);
};
} // namespace Shadowless
