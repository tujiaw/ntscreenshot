#include "modules/local_search/application/SearchProviders.h"

#include "modules/local_search/infrastructure/SearchIndexStore.h"

FileSearchProvider::FileSearchProvider(std::shared_ptr<SearchIndexStore> store)
    : store_(std::move(store)) {}

QVector<SearchResult> FileSearchProvider::search(const SearchQuery& query) const
{
    SearchQuery filtered = query;
    filtered.types = {SearchItemType::File, SearchItemType::Directory};
    return store_->search(filtered);
}

ApplicationSearchProvider::ApplicationSearchProvider(std::shared_ptr<SearchIndexStore> store)
    : store_(std::move(store)) {}

QVector<SearchResult> ApplicationSearchProvider::search(const SearchQuery& query) const
{
    SearchQuery filtered = query;
    filtered.types = {SearchItemType::Application};
    return store_->search(filtered);
}

BookmarkSearchProvider::BookmarkSearchProvider(std::shared_ptr<SearchIndexStore> store)
    : store_(std::move(store)) {}

QVector<SearchResult> BookmarkSearchProvider::search(const SearchQuery& query) const
{
    SearchQuery filtered = query;
    filtered.types = {SearchItemType::Bookmark};
    return store_->search(filtered);
}

SearchAggregator::SearchAggregator(std::shared_ptr<SearchIndexStore> store)
    : store_(std::move(store)) {}

QVector<SearchResult> SearchAggregator::search(const SearchQuery& query) const
{
    return store_->search(query);
}
