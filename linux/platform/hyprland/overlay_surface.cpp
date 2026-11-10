#include "overlay_surface.h"
#define LAYERSHELLQTINTERFACE_DISABLE_DEPRECATED_BEFORE_AND_AT 0x060600
#include <LayerShellQt/Window>
#include <QApplication>
#include <QCursor>
#include <QWindow>

namespace Shadowless {
void OverlaySurface::prepare(QWidget *widget, bool passive) {
    widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | (passive ? Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus : Qt::WindowFlags{}));
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setAttribute(Qt::WA_ShowWithoutActivating, passive);
    widget->winId();
    auto *surface = LayerShellQt::Window::get(widget->windowHandle());
    surface->setScope(passive ? "shadowless-indicator" : "shadowless-overlay");
    surface->setLayer(LayerShellQt::Window::LayerOverlay);
    surface->setExclusiveZone(-1);
    surface->setKeyboardInteractivity(passive ? LayerShellQt::Window::KeyboardInteractivityNone : LayerShellQt::Window::KeyboardInteractivityExclusive);
    surface->setActivateOnShow(!passive);
    surface->setCloseOnDismissed(false);
    if (passive) {
        surface->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorRight);
        surface->setMargins(QMargins(0, 18, 18, 0));
        surface->setDesiredSize(widget->size());
    } else {
        surface->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorBottom |
                            LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight);
        surface->setDesiredSize(QSize(0, 0));
    }
}
void OverlaySurface::show(QWidget *widget) {
    auto *screen = QApplication::screenAt(QCursor::pos());
    if (!screen) screen = QApplication::primaryScreen();
    LayerShellQt::Window::get(widget->windowHandle())->setScreen(screen);
    widget->show();
}
} // namespace Shadowless
