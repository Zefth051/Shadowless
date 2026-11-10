#pragma once
#include <QJsonObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <QUuid>

namespace Shadowless {
struct OverlayLayer {
    QString id = QUuid::createUuid().toString(QUuid::Id128), path;
    bool enabled = true;
    double x = .7, y = .7, width = .25, height = .25;
    int opacity = 100;
    void normalize();
    void setPosition(double left, double top);
    QRectF fitRect(QSize canvas, QSize media) const;
    QJsonObject toJson() const;
    static bool supports(const QString &path);
};
struct OverlayScene {
    bool enabled = false;
    QVector<OverlayLayer> layers;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &json, OverlayScene *result, QString *error);
};
} // namespace Shadowless
