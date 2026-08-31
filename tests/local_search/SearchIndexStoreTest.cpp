#include "modules/local_search/infrastructure/SearchIndexStore.h"
#include "modules/local_search/infrastructure/BrowserBookmarkReader.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>

#include <algorithm>
#include <cstdio>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "temporary directory unavailable")) return 1;

    SearchIndexStore store(directory.filePath(QStringLiteral("search.db")));
    QString error;
    if (!store.initialize(&error)) {
        std::fprintf(stderr, "INIT ERROR: '%s'\n", qPrintable(error));
        return 2;
    }

    const QByteArray bookmarkJson = R"json({
        "roots": {"bookmark_bar": {"name": "书签栏", "children": [
            {"type": "folder", "name": "开发", "children": [
                {"type": "url", "name": "Qt 文档", "url": "https://doc.qt.io/qt-6/"}
            ]},
            {"type": "url", "name": "本地脚本", "url": "javascript:void(0)"}
        ]}}
    })json";
    const QString bookmarksPath = directory.filePath(QStringLiteral("Bookmarks"));
    QFile bookmarksFile(bookmarksPath);
    if (!bookmarksFile.open(QIODevice::WriteOnly)
        || bookmarksFile.write(bookmarkJson) != bookmarkJson.size()) return 19;
    bookmarksFile.close();
    const QVector<SearchIndexItem> bookmarks =
        BrowserBookmarkReader::readFile(bookmarksPath, QStringLiteral("Chrome"));
    if (!require(bookmarks.size() == 1, "bookmark JSON parsing failed")) return 20;
    if (!require(bookmarks.first().type == SearchItemType::Bookmark
                 && bookmarks.first().parentPath.contains(QStringLiteral("开发")),
                 "bookmark metadata parsing failed")) return 21;

    SearchIndexItem item;
    item.type = SearchItemType::File;
    item.name = QStringLiteral("微信文档.docx");
    item.path = directory.filePath(item.name);
    item.parentPath = directory.path();
    item.extension = QStringLiteral("docx");
    item.sourceRoot = directory.path();
    if (!store.replaceRoot(directory.path(), {item}, &error)) {
        qCritical("%s", qPrintable(error));
        return 3;
    }

    SearchQuery query;
    query.limit = 10;
    query.text = QStringLiteral("文档");
    const auto results = store.search(query);
    if (!require(results.size() == 1, "Chinese name search failed")) return 4;

    // 拼音搜索：微信 → weixin，应命中 "微信文档.docx" 的 name_pinyin
    query.text = QStringLiteral("weixin");
    const auto pinyinResults = store.search(query);
    if (!require(pinyinResults.size() == 1, "pinyin search failed")) return 5;

    store.recordLaunch(results.first().id);
    if (!require(store.replaceRoot(directory.path(), {item}, &error), "root rebuild failed")) return 7;

    SearchIndexItem applicationItem = item;
    applicationItem.type = SearchItemType::Application;
    applicationItem.name = QStringLiteral("Calculator");
    applicationItem.launchTarget = QStringLiteral("Microsoft.WindowsCalculator_8wekyb3d8bbwe!App");
    applicationItem.sourceRoot = QStringLiteral("::applications:apps-folder::");
    if (!require(store.replaceRoot(applicationItem.sourceRoot,
                                   {applicationItem, applicationItem}, &error),
                 "application upsert or deduplication failed")) return 8;

    SearchQuery applicationQuery;
    applicationQuery.limit = 10;
    applicationQuery.types = {SearchItemType::Application};
    applicationQuery.text = QStringLiteral("calculator");
    if (!require(store.search(applicationQuery).size() == 1,
                 "application was not searchable after upsert")) return 9;

    applicationItem.name = QStringLiteral("System Calculator");
    if (!require(store.replaceRoot(applicationItem.sourceRoot, {applicationItem}, &error),
                 "application update failed")) return 10;
    applicationQuery.text = QStringLiteral("system calculator");
    if (!require(store.search(applicationQuery).size() == 1,
                 "FTS index was not updated")) return 11;

    SearchIndexItem baselineApplication = applicationItem;
    baselineApplication.name = QStringLiteral("System Calculator Alpha");
    baselineApplication.path = directory.filePath(QStringLiteral("calculator-alpha.exe"));
    baselineApplication.launchTarget = baselineApplication.path;
    SearchIndexItem frequentApplication = baselineApplication;
    frequentApplication.name = QStringLiteral("System Calculator Beta");
    frequentApplication.path = directory.filePath(QStringLiteral("calculator-preview.exe"));
    frequentApplication.launchTarget = frequentApplication.path;
    if (!require(store.replaceRoot(applicationItem.sourceRoot,
                                   {baselineApplication, frequentApplication}, &error),
                 "multiple application update failed")) return 12;
    applicationQuery.text = QStringLiteral("system calculator");
    auto rankedApplications = store.search(applicationQuery);
    if (!require(rankedApplications.size() == 2, "multiple application search failed")) return 13;
    const auto frequentResult = std::find_if(
        rankedApplications.cbegin(), rankedApplications.cend(),
        [&frequentApplication](const SearchResult& result) {
            return result.path == frequentApplication.path;
        });
    if (!require(frequentResult != rankedApplications.cend(), "frequent application missing")) return 14;
    store.recordLaunch(frequentResult->id);
    if (!require(store.replaceRoot(applicationItem.sourceRoot,
                                   {baselineApplication, frequentApplication}, &error),
                 "usage-preserving rebuild failed")) return 15;
    rankedApplications = store.search(applicationQuery);
    if (rankedApplications.isEmpty()
        || rankedApplications.first().path != frequentApplication.path) {
        for (const SearchResult& result : rankedApplications) {
            std::fprintf(stderr, "ranked: %.1f %s\n", result.score,
                         qPrintable(result.path));
        }
    }
    if (!require(!rankedApplications.isEmpty()
                     && rankedApplications.first().path == frequentApplication.path,
                 "recently opened result was not ranked first")) return 16;

    store.removeRoot(directory.path());
    if (!require(store.itemCount() == 2, "file root removal affected application record")) return 17;
    store.removeRoot(applicationItem.sourceRoot);
    if (!require(store.itemCount() == 0, "root removal left stale records")) return 18;
    if (!require(store.replaceRoot(QStringLiteral("::bookmarks::"), bookmarks, &error),
                 "bookmark indexing failed")) return 22;
    SearchQuery bookmarkQuery;
    bookmarkQuery.text = QStringLiteral("Qt 文档");
    bookmarkQuery.types = {SearchItemType::Bookmark};
    const auto bookmarkResults = store.search(bookmarkQuery);
    if (!require(bookmarkResults.size() == 1
                 && bookmarkResults.first().launchTarget.startsWith(QStringLiteral("https://")),
                 "bookmark search failed")) return 23;
    return 0;
}
