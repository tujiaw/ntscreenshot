#pragma once

#include "modules/local_search/domain/SearchTypes.h"

#include <QString>
#include <QVector>

class QSqlDatabase;

class SearchIndexStore final {
public:
    explicit SearchIndexStore(QString databasePath);

    bool initialize(QString* error = nullptr) const;
    bool replaceRoot(const QString& root, const QVector<SearchIndexItem>& items,
                     QString* error = nullptr) const;
    void removeRoot(const QString& root) const;
    QVector<SearchResult> search(const SearchQuery& query) const;
    void recordLaunch(qint64 id) const;
    qint64 itemCount() const;
    qint64 itemCountForRoot(const QString& root) const;

    static void pauseBackgroundAccess();
    static void resumeBackgroundAccess();

private:
    QSqlDatabase readConnection() const;
    QString databasePath_;
};
