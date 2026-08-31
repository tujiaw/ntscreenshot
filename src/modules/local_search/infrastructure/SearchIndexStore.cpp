#include "modules/local_search/infrastructure/SearchIndexStore.h"

#include "core/pinyin/hanzi_to_pinyin.hpp"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QHash>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace {

static std::atomic<bool> g_backgroundAccessPaused{false};

// 将名称转换为拼音（音节直接拼接，无分隔符），用于拼音搜索匹配。
// 非汉字字符（数字、英文、. 等）原样保留。
QString nameToPinyin(const QString& name)
{
    if (name.isEmpty()) return {};
    hanzi_to_pinyin::Options options;
    options.separator = "";
    options.preserve_unmapped = true;
    options.phrase_matching = true;
    const std::string utf8 = name.toUtf8().toStdString();
    auto result = hanzi_to_pinyin::convert(utf8, options);
    return result ? QString::fromUtf8(result.value.c_str()) : QString();
}

// 把搜索词归一化为小写字母数字（去掉 ' - 等分隔符），用于拼音列匹配；
// 不含字母数字（如纯中文词）时返回空。
QString pinyinMatchTerm(const QString& term)
{
    QString out;
    out.reserve(term.size());
    for (QChar ch : term) {
        const ushort u = ch.unicode();
        if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9')) {
            out.append(ch.toLower());
        }
    }
    return out;
}

void applySqlitePragmas(QSqlDatabase& db)
{
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    query.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
}

class ScopedConnection final {
public:
    explicit ScopedConnection(const QString& path)
        : name_(QStringLiteral("local_search_%1_%2")
                    .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
                    .arg(QUuid::createUuid().toString(QUuid::Id128)))
    {
        while (g_backgroundAccessPaused.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
        db_.setDatabaseName(path);
        db_.open();
        if (db_.isOpen()) {
            applySqlitePragmas(db_);
        }
    }

    ~ScopedConnection()
    {
        db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(name_);
    }

    QSqlDatabase& db() { return db_; }

private:
    QString name_;
    QSqlDatabase db_;
};

QString escapedLike(QString value)
{
    value.replace(u'\\', QStringLiteral("\\\\"));
    value.replace(u'%', QStringLiteral("\\%"));
    value.replace(u'_', QStringLiteral("\\_"));
    return value;
}

double matchScore(const SearchResult& result, const QString& query,
                  qint64 runCount, qint64 lastUsed)
{
    const QString name = result.name.toLower();
    const QString path = result.path.toLower();
    double score = result.type == SearchItemType::Application ? 240.0
                   : result.type == SearchItemType::Bookmark ? 120.0 : 0.0;
    if (name == query) score += 1000.0;
    else if (name.startsWith(query)) score += 700.0;
    else if (name.contains(query)) score += 500.0;
    if (path.contains(query)) score += 120.0;
    score += std::min<qint64>(runCount, 20) * 25.0;
    if (lastUsed > 0) {
        const qint64 ageDays = (QDateTime::currentMSecsSinceEpoch() - lastUsed) / 86400000;
        score += std::max<qint64>(0, 300 - ageDays * 10);
    }
    return score;
}

} // namespace

void SearchIndexStore::pauseBackgroundAccess()
{
    g_backgroundAccessPaused.store(true, std::memory_order_release);
}

void SearchIndexStore::resumeBackgroundAccess()
{
    g_backgroundAccessPaused.store(false, std::memory_order_release);
}

SearchIndexStore::SearchIndexStore(QString databasePath)
    : databasePath_(std::move(databasePath))
{
    qInfo() << "SearchIndexStore: created with path =" << databasePath_;
}

bool SearchIndexStore::initialize(QString* error) const
{
    qInfo() << "SearchIndexStore::initialize: path =" << databasePath_;
    QDir().mkpath(QFileInfo(databasePath_).absolutePath());
    ScopedConnection connection(databasePath_);
    QSqlDatabase& db = connection.db();
    if (!db.isOpen()) {
        if (error) *error = db.lastError().text();
        qWarning() << "SearchIndexStore::initialize: failed to open database:" << *error;
        return false;
    }

    QSqlQuery versionQuery(db);
    versionQuery.exec(QStringLiteral("PRAGMA user_version"));
    const int schemaVersion = versionQuery.next() ? versionQuery.value(0).toInt() : 0;
    qInfo() << "SearchIndexStore::initialize: schema version =" << schemaVersion;
    if (schemaVersion != 2) {
        qInfo() << "SearchIndexStore::initialize: schema upgrade needed, dropping old tables...";
        if (!db.transaction()) {
            if (error) *error = db.lastError().text();
            return false;
        }
        const QStringList resetStatements = {
            QStringLiteral("DROP TRIGGER IF EXISTS search_items_ai"),
            QStringLiteral("DROP TRIGGER IF EXISTS search_items_ad"),
            QStringLiteral("DROP TRIGGER IF EXISTS search_items_au"),
            QStringLiteral("DROP TABLE IF EXISTS search_items_fts"),
            QStringLiteral("DROP TABLE IF EXISTS search_items")
        };
        for (const QString& statement : resetStatements) {
            QSqlQuery query(db);
            if (!query.exec(statement)) {
                if (error) *error = query.lastError().text();
                db.rollback();
                return false;
            }
        }
        if (!db.commit()) {
            if (error) *error = db.lastError().text();
            return false;
        }
    }
    const QStringList schema = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS search_items ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, type INTEGER NOT NULL,"
                       "name TEXT NOT NULL, path TEXT NOT NULL UNIQUE, parent_path TEXT,"
                       "extension TEXT, modified_at INTEGER NOT NULL DEFAULT 0,"
                       "launch_target TEXT, name_pinyin TEXT,"
                       "source_root TEXT NOT NULL, run_count INTEGER NOT NULL DEFAULT 0,"
                       "last_used INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_search_items_root ON search_items(source_root)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_search_items_name ON search_items(name COLLATE NOCASE)"),
        QStringLiteral("CREATE VIRTUAL TABLE IF NOT EXISTS search_items_fts USING fts5("
                       "name, path, name_pinyin, content='search_items',"
                       "content_rowid='id', tokenize='trigram')"),
        QStringLiteral("CREATE TRIGGER IF NOT EXISTS search_items_ai AFTER INSERT ON search_items BEGIN "
                       "INSERT INTO search_items_fts(rowid,name,path,name_pinyin) "
                       "VALUES(new.id,new.name,new.path,new.name_pinyin); END"),
        QStringLiteral("CREATE TRIGGER IF NOT EXISTS search_items_ad AFTER DELETE ON search_items BEGIN "
                       "INSERT INTO search_items_fts(search_items_fts,rowid,name,path,name_pinyin) "
                       "VALUES('delete',old.id,old.name,old.path,old.name_pinyin); END"),
        QStringLiteral("CREATE TRIGGER IF NOT EXISTS search_items_au AFTER UPDATE ON search_items BEGIN "
                       "INSERT INTO search_items_fts(search_items_fts,rowid,name,path,name_pinyin) "
                       "VALUES('delete',old.id,old.name,old.path,old.name_pinyin); "
                       "INSERT INTO search_items_fts(rowid,name,path,name_pinyin) "
                       "VALUES(new.id,new.name,new.path,new.name_pinyin); END")
    };
    for (const QString& statement : schema) {
        QSqlQuery query(db);
        if (!query.exec(statement)) {
            if (error) *error = query.lastError().text();
            return false;
        }
    }
    QSqlQuery updateVersion(db);
    if (!updateVersion.exec(QStringLiteral("PRAGMA user_version=3"))) {
        if (error) *error = updateVersion.lastError().text();
        qWarning() << "SearchIndexStore::initialize: failed to update schema version:" << *error;
        return false;
    }
    qInfo() << "SearchIndexStore::initialize: done, item count =" << itemCount();
    return true;
}

bool SearchIndexStore::replaceRoot(const QString& root, const QVector<SearchIndexItem>& items,
                                   QString* error) const
{
    qInfo() << "SearchIndexStore::replaceRoot: root =" << root << "items =" << items.size();
    if (error) error->clear();
    ScopedConnection connection(databasePath_);
    QSqlDatabase& db = connection.db();
    if (!db.isOpen() || !db.transaction()) {
        qWarning() << "SearchIndexStore::replaceRoot: failed to open/transaction:" << root;
        return false;
    }

    QVector<SearchIndexItem> uniqueItems;
    QHash<QString, qsizetype> itemIndexes;
    uniqueItems.reserve(items.size());
    for (const SearchIndexItem& item : items) {
        if (item.path.trimmed().isEmpty()) continue;
        const QString key = QDir::cleanPath(item.path).toCaseFolded();
        const auto existingIndex = itemIndexes.constFind(key);
        if (existingIndex == itemIndexes.cend()) {
            itemIndexes.insert(key, uniqueItems.size());
            uniqueItems.push_back(item);
        } else {
            uniqueItems[*existingIndex] = item;
        }
    }

    QHash<QString, QPair<qint64, qint64>> usage;
    QSqlQuery existing(db);
    existing.prepare(QStringLiteral(
        "SELECT path,run_count,last_used FROM search_items WHERE source_root=?"));
    existing.addBindValue(root);
    if (existing.exec()) {
        while (existing.next()) {
            usage.insert(existing.value(0).toString(),
                         {existing.value(1).toLongLong(), existing.value(2).toLongLong()});
        }
    }

    QSqlQuery createIncoming(db);
    if (!createIncoming.exec(QStringLiteral(
            "CREATE TEMP TABLE incoming_search_paths(path TEXT PRIMARY KEY)"))) {
        if (error) *error = createIncoming.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery addIncoming(db);
    addIncoming.prepare(QStringLiteral("INSERT OR IGNORE INTO incoming_search_paths(path) VALUES(?)"));
    for (const SearchIndexItem& item : uniqueItems) {
        addIncoming.bindValue(0, item.path);
        if (!addIncoming.exec()) {
            if (error) *error = addIncoming.lastError().text();
            db.rollback();
            return false;
        }
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO search_items(type,name,path,parent_path,extension,modified_at,"
        "launch_target,name_pinyin,source_root,run_count,last_used) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(path) DO UPDATE SET "
        "type=excluded.type,name=excluded.name,parent_path=excluded.parent_path,"
        "extension=excluded.extension,modified_at=excluded.modified_at,"
        "launch_target=excluded.launch_target,name_pinyin=excluded.name_pinyin,"
        "source_root=excluded.source_root "
        "WHERE excluded.type=2 OR search_items.type<>2"));
    int successfulStatements = 0;
    QString firstInsertError;
    for (const SearchIndexItem& item : uniqueItems) {
        insert.bindValue(0, static_cast<int>(item.type));
        insert.bindValue(1, item.name);
        insert.bindValue(2, item.path);
        insert.bindValue(3, item.parentPath);
        insert.bindValue(4, item.extension);
        insert.bindValue(5, item.modifiedAt);
        insert.bindValue(6, item.launchTarget);
        insert.bindValue(7, nameToPinyin(item.name));
        insert.bindValue(8, root);
        const auto previousUsage = usage.value(item.path);
        insert.bindValue(9, previousUsage.first);
        insert.bindValue(10, previousUsage.second);
        if (!insert.exec()) {
            if (firstInsertError.isEmpty()) firstInsertError = insert.lastError().text();
            continue;
        }
        ++successfulStatements;
    }
    if (!uniqueItems.isEmpty() && successfulStatements == 0) {
        if (error) *error = firstInsertError;
        db.rollback();
        return false;
    }

    QSqlQuery removeStale(db);
    removeStale.prepare(QStringLiteral(
        "DELETE FROM search_items WHERE source_root=? AND NOT EXISTS ("
        "SELECT 1 FROM incoming_search_paths WHERE incoming_search_paths.path=search_items.path)"));
    removeStale.addBindValue(root);
    if (!removeStale.exec()) {
        if (error) *error = removeStale.lastError().text();
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        qWarning() << "SearchIndexStore::replaceRoot: commit failed for root =" << root << ":" << *error;
        return false;
    }
    qInfo() << "SearchIndexStore::replaceRoot: done for root =" << root
            << "items inserted =" << successfulStatements;
    return true;
}

void SearchIndexStore::removeRoot(const QString& root) const
{
    ScopedConnection connection(databasePath_);
    QSqlQuery query(connection.db());
    query.prepare(QStringLiteral("DELETE FROM search_items WHERE source_root=?"));
    query.addBindValue(root);
    query.exec();
}

QSqlDatabase SearchIndexStore::readConnection() const
{
    while (g_backgroundAccessPaused.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const QString connName = QStringLiteral("lsearch_r_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (db.isOpen()) return db;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(databasePath_);
    db.open();
    if (db.isOpen()) {
        applySqlitePragmas(db);
    }
    return db;
}

QVector<SearchResult> SearchIndexStore::search(const SearchQuery& query) const
{
    const QString text = query.text.trimmed();
    if (text.isEmpty()) return {};

    const QStringList tokens = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                          Qt::SkipEmptyParts);
    if (tokens.isEmpty()) return {};

    QSqlDatabase db = readConnection();
    if (!db.isOpen()) return {};

    const QString columns = QStringLiteral(
        "search_items.id,search_items.type,search_items.name,search_items.path,"
        "search_items.parent_path,search_items.launch_target,"
        "search_items.run_count,search_items.last_used");

    const QString typeFilter = [&query]() -> QString {
        if (query.types.isEmpty()) return {};
        QStringList types;
        for (SearchItemType type : query.types)
            types << QString::number(static_cast<int>(type));
        return QStringLiteral(" AND type IN (%1)").arg(types.join(','));
    }();

    // Separate into FTS-eligible and LIKE-only terms.
    QStringList ftsTerms, likeTerms;
    for (const QString& token : tokens) {
        const QString term = token.toLower();
        if (term.isEmpty()) continue;
        if (term.size() >= 3) ftsTerms << term;
        else likeTerms << term;
    }

    QHash<qint64, SearchResult> accumulated;
    bool first = true;

    // Process each FTS term individually, intersecting as we go.
    // FTS5 trigram tokenizer has no native boolean AND; code-level
    // intersection is the reliable cross-version approach.
    for (const QString& term : ftsTerms) {
        QHash<qint64, double> termScores;
        bool gotResults = false;

        QString sql = QStringLiteral(
            "SELECT %1 FROM search_items JOIN search_items_fts "
            "ON search_items_fts.rowid=search_items.id WHERE search_items_fts MATCH ?")
                          .arg(columns) + typeFilter + QStringLiteral(" LIMIT ?");
        QSqlQuery stmt(db);
        stmt.prepare(sql);
        QString ftsText = term;
        ftsText.replace(u'"', QStringLiteral("\"\""));
        QString matchExpr;
        if (query.pinyinEnabled) {
            matchExpr = QStringLiteral("\"") + ftsText + QStringLiteral("\"");
        } else {
            // 关闭拼音时只匹配 name/path 列，排除 name_pinyin
            matchExpr = QStringLiteral("name:\"") + ftsText
                        + QStringLiteral("\" OR path:\"") + ftsText + QStringLiteral("\"");
        }
        stmt.addBindValue(matchExpr);
        stmt.addBindValue(qMax(query.limit * 8, 200));
        if (stmt.exec() && stmt.isActive()) {
            while (stmt.next()) {
                SearchResult r;
                r.id = stmt.value(0).toLongLong();
                r.type = static_cast<SearchItemType>(stmt.value(1).toInt());
                r.name = stmt.value(2).toString();
                r.path = stmt.value(3).toString();
                r.parentPath = stmt.value(4).toString();
                r.launchTarget = stmt.value(5).toString();
                r.score = matchScore(r, term, stmt.value(6).toLongLong(),
                                     stmt.value(7).toLongLong());
                termScores[r.id] = r.score;
                if (first) accumulated[r.id] = r;
            }
            gotResults = true;
        }

        // FTS failed for this term — queue it as a LIKE term instead.
        if (!gotResults) likeTerms << term;

        if (!first) {
            QHash<qint64, SearchResult> intersection;
            for (auto it = termScores.begin(); it != termScores.end(); ++it) {
                auto existing = accumulated.find(it.key());
                if (existing != accumulated.end()) {
                    existing->score += it.value();
                    intersection[it.key()] = existing.value();
                }
            }
            accumulated = intersection;
        }
        first = false;

        if (accumulated.isEmpty() && !ftsTerms.isEmpty()) break;
    }

    // Combine all LIKE terms into a single query — one table scan instead of N.
    if (!likeTerms.isEmpty()) {
        if (accumulated.isEmpty()) first = true;

        const int limit = qMax(query.limit * 8, 200);
        QStringList conditions;
        QStringList bindPatterns;
        for (const QString& term : likeTerms) {
            const QString pattern = QStringLiteral("%") + escapedLike(term) + QStringLiteral("%");
            const QString pinyinTerm = pinyinMatchTerm(term);
            if (!query.pinyinEnabled || pinyinTerm.isEmpty()) {
                conditions << QStringLiteral(
                    "(lower(name) LIKE ? ESCAPE '\\' OR lower(path) LIKE ? ESCAPE '\\')");
                bindPatterns << pattern << pattern;
            } else {
                const QString pinyinPattern =
                    QStringLiteral("%") + escapedLike(pinyinTerm) + QStringLiteral("%");
                conditions << QStringLiteral(
                    "(lower(name) LIKE ? ESCAPE '\\' OR lower(path) LIKE ? ESCAPE '\\'"
                    " OR lower(name_pinyin) LIKE ? ESCAPE '\\')");
                bindPatterns << pattern << pattern << pinyinPattern;
            }
        }
        QString sql = QStringLiteral("SELECT %1 FROM search_items WHERE ")
                          .arg(columns)
                      + conditions.join(QStringLiteral(" AND "))
                      + typeFilter + QStringLiteral(" LIMIT ?");
        QSqlQuery stmt(db);
        stmt.prepare(sql);
        for (const QString& pattern : bindPatterns) {
            stmt.addBindValue(pattern);
        }
        stmt.addBindValue(limit);

        if (stmt.exec() && stmt.isActive()) {
            QHash<qint64, double> likeScores;
            QHash<qint64, SearchResult> likeResults;
            while (stmt.next()) {
                SearchResult r;
                r.id = stmt.value(0).toLongLong();
                r.type = static_cast<SearchItemType>(stmt.value(1).toInt());
                r.name = stmt.value(2).toString();
                r.path = stmt.value(3).toString();
                r.parentPath = stmt.value(4).toString();
                r.launchTarget = stmt.value(5).toString();
                const qint64 runCount = stmt.value(6).toLongLong();
                const qint64 lastUsed = stmt.value(7).toLongLong();
                double combinedScore = 0;
                for (const QString& term : likeTerms) {
                    combinedScore += matchScore(r, term, runCount, lastUsed);
                }
                likeScores[r.id] = combinedScore;
                likeResults[r.id] = r;
                r.score = combinedScore;
            }

            if (first) {
                accumulated = likeResults;
            } else {
                QHash<qint64, SearchResult> intersection;
                for (auto it = likeScores.begin(); it != likeScores.end(); ++it) {
                    auto existing = accumulated.find(it.key());
                    if (existing != accumulated.end()) {
                        existing->score += it.value();
                        intersection[it.key()] = existing.value();
                    }
                }
                accumulated = intersection;
            }
        }
    }

    QVector<SearchResult> results;
    for (auto& pair : accumulated) results.append(pair);

    std::sort(results.begin(), results.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
    });
    if (results.size() > query.limit) results.resize(query.limit);
    return results;
}

void SearchIndexStore::recordLaunch(qint64 id) const
{
    ScopedConnection connection(databasePath_);
    QSqlQuery query(connection.db());
    query.prepare(QStringLiteral("UPDATE search_items SET run_count=run_count+1,last_used=? WHERE id=?"));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(id);
    query.exec();
}

qint64 SearchIndexStore::itemCount() const
{
    ScopedConnection connection(databasePath_);
    QSqlQuery query(QStringLiteral("SELECT COUNT(*) FROM search_items"), connection.db());
    return query.next() ? query.value(0).toLongLong() : 0;
}

qint64 SearchIndexStore::itemCountForRoot(const QString& root) const
{
    ScopedConnection connection(databasePath_);
    QSqlQuery query(connection.db());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM search_items WHERE source_root=?"));
    query.addBindValue(root);
    return query.exec() && query.next() ? query.value(0).toLongLong() : 0;
}
