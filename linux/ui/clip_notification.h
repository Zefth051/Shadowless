#pragma once
#include <QWidget>
#include <QVariantAnimation>
#include <QColor>
class QLabel;
namespace Shadowless {
class ClipNotification : public QWidget {
public:
    enum Kind { Saved, Saving, Failed };
    ClipNotification();
    void present(const QString &title, const QString &detail, int timeoutMs = 3500, Kind kind = Saved);
    void setAccent(const QColor &accent) { m_accent = accent; update(); }
    void dismiss();
protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    QLabel *m_title, *m_detail;
    QVariantAnimation m_lifetime{this};
    Kind m_kind = Saved;
    QColor m_accent = QColor("#63e0bd");
};
}
