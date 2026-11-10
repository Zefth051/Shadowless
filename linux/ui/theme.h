#pragma once
#include <QColor>
#include <QIcon>
#include <QStringList>
class QWidget;
namespace Shadowless {
QStringList accentNames();
QColor accentColor(const QString &name);
QIcon accentIcon(const QString &name);
void applyTheme(QWidget *widget, const QString &accent = "mint");
}
