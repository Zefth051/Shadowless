#pragma once
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>

namespace Shadowless {
// "Replay_2026-11-16_21-00-42.mp4" -> type "Replay", moment "Nov 16, 9:00 PM". Unrecognised names fall back to the file name and mtime.
inline QString clipTitle(const QFileInfo &file) {
    const auto match = QRegularExpression("^(Replay|Recording)_", QRegularExpression::CaseInsensitiveOption).match(file.completeBaseName());
    return match.hasMatch() ? match.captured(1) : file.completeBaseName();
}
inline QString clipWhen(const QFileInfo &file) {
    auto moment = QDateTime::fromString(file.completeBaseName().section('_', 1, 2), "yyyy-MM-dd_HH-mm-ss");
    if (!moment.isValid()) moment = file.lastModified();
    return QLocale().toString(moment.date(), "MMM d") + ", " + QLocale().toString(moment.time(), QLocale::ShortFormat);
}
}
