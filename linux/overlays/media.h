#pragma once
#include <QImage>
#include <QString>
#include <memory>

namespace Shadowless {
// Worker-thread decoder; it never opens audio devices or decodes audio streams.
class Media {
public:
    Media();
    ~Media();
    bool open(const QString &path, QSize maximum, QString *error);
    bool advance(qint64 elapsedMs);
    const QImage &frame() const;
    bool animated() const;
private:
    struct Data;
    std::unique_ptr<Data> d;
};
} // namespace Shadowless
