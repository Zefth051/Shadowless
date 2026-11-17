#pragma once
#include "../overlays/input/core/input_renderer.h"
#include "../overlays/input/core/input_timeline.h"
#include "../settings/settings.h"
#include <QWidget>

namespace Shadowless {
// Draws the recorded peripheral overlay over the (clean) video in the clip player. It never touches
// the video file: it is rebuilt from `<clip>.inputs.json` and follows the playback position, so it
// can be hidden, resized or moved without altering the original footage.
class InputOverlayLayer : public QWidget {
    Q_OBJECT
public:
    explicit InputOverlayLayer(QWidget *parent = nullptr);
    bool load(const QString &clipPath); // false when the clip has no (valid) sidecar
    void clear();
    bool hasTimeline() const { return m_timeline.isValid(); }
    void setSettings(const InputOverlaySettings &settings);
    void setPositionMs(qint64 ms);
    void place(const QRect &video); // geometry of the video inside the parent
protected:
    void paintEvent(QPaintEvent *) override;
private:
    Input::InputTimeline m_timeline;
    Input::Layout m_layout;
    Input::InputState m_state;
    InputOverlaySettings m_settings;
    QRect m_video;
};
} // namespace Shadowless
