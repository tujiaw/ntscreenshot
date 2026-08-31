#pragma once

#include <QString>
#include <QVector>

enum class SearchItemType {
    File = 0,
    Directory = 1,
    Application = 2,
    Bookmark = 3
};

struct SearchQuery {
    QString text;
    int limit = 50;
    QVector<SearchItemType> types;
    bool pinyinEnabled = true;   // 是否启用中文拼音匹配
};

struct SearchResult {
    qint64 id = 0;
    SearchItemType type = SearchItemType::File;
    QString name;
    QString path;
    QString parentPath;
    QString launchTarget;
    double score = 0.0;
};

struct SearchIndexItem {
    SearchItemType type = SearchItemType::File;
    QString name;
    QString path;
    QString parentPath;
    QString extension;
    QString launchTarget;
    QString sourceRoot;
    qint64 modifiedAt = 0;
};
