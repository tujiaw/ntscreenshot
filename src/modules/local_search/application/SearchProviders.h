#pragma once

#include "modules/local_search/domain/ISearchProvider.h"

#include <memory>

class SearchIndexStore;

class FileSearchProvider final : public ISearchProvider {
public:
    explicit FileSearchProvider(std::shared_ptr<SearchIndexStore> store);
    QVector<SearchResult> search(const SearchQuery& query) const override;
private:
    std::shared_ptr<SearchIndexStore> store_;
};

class ApplicationSearchProvider final : public ISearchProvider {
public:
    explicit ApplicationSearchProvider(std::shared_ptr<SearchIndexStore> store);
    QVector<SearchResult> search(const SearchQuery& query) const override;
private:
    std::shared_ptr<SearchIndexStore> store_;
};

class BookmarkSearchProvider final : public ISearchProvider {
public:
    explicit BookmarkSearchProvider(std::shared_ptr<SearchIndexStore> store);
    QVector<SearchResult> search(const SearchQuery& query) const override;
private:
    std::shared_ptr<SearchIndexStore> store_;
};

class SearchAggregator final {
public:
    explicit SearchAggregator(std::shared_ptr<SearchIndexStore> store);
    QVector<SearchResult> search(const SearchQuery& query) const;
private:
    std::shared_ptr<SearchIndexStore> store_;
};
