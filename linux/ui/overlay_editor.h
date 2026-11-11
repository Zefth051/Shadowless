#pragma once
#include "../overlays/layer.h"
#include "../overlays/media.h"
#include "../overlays/preview_client.h"
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <map>
class QListWidget;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QStackedWidget;
class QLabel;
class QLineEdit;
namespace Shadowless {
class OverlayCanvas : public QWidget {
    Q_OBJECT
public:
    explicit OverlayCanvas(QWidget *parent = nullptr);
    void setScene(const OverlayScene &scene, int selected);
    void setPreviewSocket(const QString &path);
    void setCaptureVisible(bool visible);
    QString mediaError() const { return m_error; }
signals:
    void moved(int row, double x, double y);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    OverlayScene m_scene;
    int m_selected = -1;
    QPointF m_drag;
    QTimer m_timer;
    QElapsedTimer m_clock;
    std::map<QString, std::pair<QString, std::shared_ptr<Media>>> m_media;
    QString m_error;
    PreviewClient m_preview;
    QImage m_capture;
    bool m_captureVisible = true;
    QSize m_sourceSize{1920, 1080};
    QString m_previewStatus = "Capture is off · layer editing remains available.";
    QRectF canvasRect() const;
};
class OverlayEditor : public QWidget {
    Q_OBJECT
public:
    explicit OverlayEditor(QWidget *parent = nullptr);
    void edit(const OverlayScene &scene);
    void setPreviewSocket(const QString &path);
    void setResult(bool success, const QString &error);
    const OverlayScene &scene() const { return m_scene; }
    void flush();
signals:
    void changed(const Shadowless::OverlayScene &scene);
    void back();
private:
    OverlayScene m_scene;
    QListWidget *m_layers;
    QCheckBox *m_master, *m_visible;
    QDoubleSpinBox *m_x, *m_y, *m_width, *m_height;
    QSpinBox *m_opacity;
    QLabel *m_status;
    QLineEdit *m_path;
    OverlayCanvas *m_canvas;
    QStackedWidget *m_pages;
    QWidget *m_fields;
    QTimer m_debounce;
    bool m_loading = false;
    void refreshList(int selected);
    void select(int row);
    void updateLayer();
    void schedule();
};
} // namespace Shadowless
