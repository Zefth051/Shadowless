#pragma once
#include <QString>
namespace Shadowless {
QString ensureDesktopEntry(const QString &executable);
QString configureAutostart(bool enabled, const QString &executable);
}
