#pragma once

#include <QDateTime>
#include <QString>

namespace TimeUtil {

// Returns a compact relative-time label like "now", "5m", "3h", "2d", "12-31".
inline QString FormatRelativeTime(const QDateTime& capturedAt) {
    const QDateTime now = QDateTime::currentDateTime();
    if (!capturedAt.isValid() || capturedAt >= now) {
        return QStringLiteral("now");
    }

    const qint64 secs = capturedAt.secsTo(now);
    if (secs < 60) {
        return QStringLiteral("now");
    }
    const qint64 mins = secs / 60;
    if (mins < 60) {
        return QStringLiteral("%1m").arg(mins);
    }
    const qint64 hours = secs / 3600;
    if (hours < 24) {
        return QStringLiteral("%1h").arg(hours);
    }
    const qint64 days = hours / 24;
    if (days < 7) {
        return QStringLiteral("%1d").arg(days);
    }

    const QDate date = capturedAt.date();
    const QDate today = now.date();
    if (date.year() == today.year()) {
        return QStringLiteral("%1-%2").arg(date.month()).arg(date.day());
    }
    return QStringLiteral("%1-%2")
        .arg(date.year() % 100, 2, 10, QChar('0'))
        .arg(date.month(), 2, 10, QChar('0'));
}

} // namespace TimeUtil
