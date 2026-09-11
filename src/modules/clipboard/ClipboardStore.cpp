#include "ClipboardStore.h"

#include <QDateTime>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <windows.h>

namespace {
constexpr char kConnectionName[] = "clipboardlite";
} // namespace

ClipboardStore::ClipboardStore() {
    Open();
}

ClipboardStore::~ClipboardStore() {
    Close();
}

std::vector<ClipItem> ClipboardStore::Load(size_t maxItems) {
    std::vector<ClipItem> items;
    if (!available_) {
        return items;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "SELECT kind, captured_at, text, dib, width, height, pinned "
        "FROM clips WHERE pinned = 1 OR id IN (SELECT id FROM clips WHERE pinned = 0 ORDER BY sort_order ASC, id ASC LIMIT ?) ORDER BY pinned DESC, sort_order ASC, id ASC"));
    query.addBindValue(static_cast<qint64>(maxItems));
    if (!query.exec()) {
        return items;
    }

    while (query.next()) {
        ClipItem item;
        item.kind = query.value(0).toInt() == 1 ? ClipKind::Image : ClipKind::Text;
        item.capturedAt = QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong());
        item.text = query.value(2).toString();
        item.data = query.value(3).toByteArray();
        item.width = query.value(4).toInt();
        item.height = query.value(5).toInt();
        item.pinned = query.value(6).toBool();

        if (item.kind == ClipKind::Text && item.text.isEmpty()) {
            continue;
        }
        if (item.kind == ClipKind::Image && item.data.isEmpty()) {
            continue;
        }
        items.push_back(std::move(item));
    }
    return items;
}

bool ClipboardStore::Save(const std::vector<ClipItem>& items, size_t maxItems) {
    if (!available_) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery clear(db);
    if (!clear.exec(QStringLiteral("DELETE FROM clips"))) {
        db.rollback();
        return false;
    }

    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO clips (kind, captured_at, text, dib, width, height, content_hash, sort_order, pinned) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"))) {
        db.rollback();
        return false;
    }

    size_t unpinnedCount = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const ClipItem& item = items[i];
        if (!item.pinned && unpinnedCount++ >= maxItems) continue;
        const int kind = item.kind == ClipKind::Image ? 1 : 0;
        const qint64 captured = item.capturedAt.toMSecsSinceEpoch();
        const QString text = item.kind == ClipKind::Text ? item.text : QString();
        insert.addBindValue(kind);
        insert.addBindValue(captured);
        if (item.kind == ClipKind::Text) {
            insert.addBindValue(text);
            insert.addBindValue(QVariant());
        } else {
            insert.addBindValue(QVariant());
            insert.addBindValue(item.data);
        }
        insert.addBindValue(item.width);
        insert.addBindValue(item.height);
        insert.addBindValue(QLatin1String(""));
        insert.addBindValue(static_cast<qint64>(i));
        insert.addBindValue(item.pinned ? 1 : 0);
        if (!insert.exec()) {
            db.rollback();
            return false;
        }
        insert.finish();
    }

    if (!db.commit()) {
        db.rollback();
        return false;
    }
    return true;
}

int ClipboardStore::LoadSettingInt(const char* name, int defaultValue) {
    if (!available_ || !name) {
        return defaultValue;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("SELECT value_int FROM settings WHERE name = ?"));
    query.addBindValue(QLatin1String(name));
    if (!query.exec() || !query.next()) {
        return defaultValue;
    }
    return query.value(0).toInt();
}

bool ClipboardStore::SaveSettingInt(const char* name, int value) {
    if (!available_ || !name) {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO settings (name, value_int, updated_at) VALUES (?, ?, strftime('%s','now') * 1000) "
        "ON CONFLICT(name) DO UPDATE SET value_int = excluded.value_int, updated_at = excluded.updated_at"));
    query.addBindValue(QLatin1String(name));
    query.addBindValue(value);
    return query.exec();
}

QString ClipboardStore::LoadSettingText(const char* name, const QString& defaultValue) {
    if (!available_ || !name) {
        return defaultValue;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("SELECT value_text FROM settings WHERE name = ?"));
    query.addBindValue(QLatin1String(name));
    if (!query.exec() || !query.next()) {
        return defaultValue;
    }
    return query.value(0).toString();
}

bool ClipboardStore::SaveSettingText(const char* name, const QString& value) {
    if (!available_ || !name) {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO settings (name, value_text, updated_at) VALUES (?, ?, strftime('%s','now') * 1000) "
        "ON CONFLICT(name) DO UPDATE SET value_text = excluded.value_text, updated_at = excluded.updated_at"));
    query.addBindValue(QLatin1String(name));
    query.addBindValue(value);
    return query.exec();
}

bool ClipboardStore::Open() {
    const QString path = DefaultDatabasePath();
    if (path.isEmpty()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(kConnectionName);
        return false;
    }
    db.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    db.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    if (!EnsureSchema()) {
        Close();
        return false;
    }
    available_ = true;
    return true;
}

bool ClipboardStore::EnsureSchema() {
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS clips ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "kind INTEGER NOT NULL,"
            "captured_at INTEGER NOT NULL,"
            "text TEXT,"
            "dib BLOB,"
            "width INTEGER NOT NULL DEFAULT 0,"
            "height INTEGER NOT NULL DEFAULT 0,"
            "content_hash TEXT NOT NULL,"
            "sort_order INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings ("
            "name TEXT PRIMARY KEY,"
            "value_int INTEGER,"
            "value_text TEXT,"
            "updated_at INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clips_sort ON clips(sort_order ASC, id ASC)"),

    };
    for (const QString& statement : statements) {
        QSqlQuery query(db);
        if (!query.exec(statement)) {
            return false;
        }
    }
    QSqlQuery columns(db);
    if (!columns.exec(QStringLiteral("PRAGMA table_info(clips)"))) return false;
    bool hasPinned = false;
    while (columns.next()) {
        if (columns.value(1).toString() == QStringLiteral("pinned")) hasPinned = true;
    }
    columns.finish();
    if (!hasPinned) {
        QSqlQuery migration(db);
        if (!migration.exec(QStringLiteral("ALTER TABLE clips ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0"))) return false;
    }
    QSqlQuery version(db);
    return version.exec(QStringLiteral("PRAGMA user_version=3"));
}

void ClipboardStore::Close() {
    if (available_) {
        QSqlDatabase::removeDatabase(kConnectionName);
        available_ = false;
    }
}

QString ClipboardStore::DefaultDatabasePath() {
    wchar_t localAppData[MAX_PATH] = {0};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData,
                                                 static_cast<DWORD>(std::size(localAppData)));
    if (length == 0 || length >= std::size(localAppData)) {
        return {};
    }

    const QString directory = QString::fromWCharArray(localAppData) + QStringLiteral("/ClipboardLite");
    if (!QDir().mkpath(directory)) {
        return {};
    }
    return directory + QStringLiteral("/history.db");
}
