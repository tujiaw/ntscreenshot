#pragma once

#include "modules/local_search/domain/SearchTypes.h"

#include <QVector>

class ISearchProvider {
public:
    virtual ~ISearchProvider() = default;
    virtual QVector<SearchResult> search(const SearchQuery& query) const = 0;
};
