#pragma once

#include "modules/local_search/domain/SearchTypes.h"

#include <QStringList>
#include <QVector>

class BrowserBookmarkReader final {
public:
    static QVector<SearchIndexItem> readSelected(const QStringList& sources);
    static QVector<SearchIndexItem> readFile(const QString& filePath,
                                             const QString& browserName);
};
