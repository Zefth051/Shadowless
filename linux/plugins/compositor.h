#pragma once
#include "../overlays/layer.h"
#include <QImage>
#include <memory>

namespace Shadowless {
struct RenderLayer { OverlayLayer layer; QImage image; };
// All file access and decoding live in this worker, never in draw().
class CompositorWorker {
public:
    CompositorWorker();
    ~CompositorWorker();
    bool start(const QString &socketPath, QSize canvas, QString *error);
    bool snapshot(QVector<RenderLayer> *layers);
    bool previewRequested() const;
    bool publishPreview(const QImage &image);
private:
    struct Data;
    std::unique_ptr<Data> d;
};
} // namespace Shadowless
