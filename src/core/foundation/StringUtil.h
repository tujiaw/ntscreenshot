#pragma once

#include <QString>

namespace StringUtil {

inline QString NormalizeForSearch(const QString& text) {
    return text.toLower();
}

inline bool MatchesSearch(const QString& text, const QString& query) {
    if (query.isEmpty()) {
        return true;
    }
    return NormalizeForSearch(text).contains(NormalizeForSearch(query), Qt::CaseInsensitive);
}

// Replace control characters with spaces and truncate for compact display.
inline QString Preview(const QString& text, int maxLength = 260) {
    QString preview = text;
    preview.replace(QChar('\r'), QChar(' '));
    preview.replace(QChar('\n'), QChar(' '));
    preview.replace(QChar('\t'), QChar(' '));
    if (preview.size() > maxLength) {
        preview.truncate(maxLength);
        preview += QStringLiteral("...");
    }
    return preview;
}

} // namespace StringUtil
