#pragma once
#include "../overlays/input/core/input_renderer.h"
#include <QWidget>

namespace Shadowless {
// Settings preview of the peripheral overlay: a dark card showing the selected layout with a few
// neutral (nothing pressed) unless live state is supplied. Never invents key presses or reads input itself.
class InputOverlayPreview : public QWidget {
    Q_OBJECT
public:
    explicit InputOverlayPreview(QWidget *parent = nullptr);
    void setLayoutId(const QString &id);
    QString layoutId() const { return m_id; }
    void setOpacity(int percent);
    void setLiveState(const Input::InputState &state);
    void clearLiveState();
    bool hasLayout() const { return m_layout.isValid(); }
    QSize sizeHint() const override { return {320, 150}; }
    QSize minimumSizeHint() const override { return {200, 120}; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_id = "none";
    Input::Layout m_layout;
    Input::InputState m_live;
    Input::RenderStyle m_style;
};
} // namespace Shadowless
