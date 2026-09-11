#include "modules/clipboard/ClipboardHistory.h"
#include "modules/clipboard/ClipboardStore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ClipboardHistory h;
    h.SetMaxItems(2);
    h.AddText("pinned");
    CHECK(h.TogglePinned(0));
    h.AddText("old"); h.AddText("recent"); h.AddText("new");
    CHECK(h.Items().size() == 3);
    CHECK(h.Items()[0].text == "pinned");
    h.AddText("recent");
    CHECK(h.Items()[0].pinned && h.Items()[1].text == "recent");
    h.AddImage(QByteArray("image"), 1, 1, 42);
    CHECK(h.TogglePinned(1));
    h.AddText("pinned"); h.AddImage(QByteArray("image"), 1, 1, 42);
    CHECK(h.Items()[0].kind == ClipKind::Image && h.Items()[1].text == "pinned");
    h.SetMaxItems(1);
    CHECK(h.Items().size() == 3);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    qputenv("LOCALAPPDATA", dir.path().toLocal8Bit());
    QDir().mkpath(dir.path() + "/ClipboardLite");
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "legacy-test");
        db.setDatabaseName(dir.path() + "/ClipboardLite/history.db");
        CHECK(db.open());
        CHECK(QSqlQuery(db).exec("CREATE TABLE clips (id INTEGER PRIMARY KEY AUTOINCREMENT, kind INTEGER NOT NULL, captured_at INTEGER NOT NULL, text TEXT, dib BLOB, width INTEGER DEFAULT 0, height INTEGER DEFAULT 0, content_hash TEXT NOT NULL, sort_order INTEGER DEFAULT 0)"));
    }
    QSqlDatabase::removeDatabase("legacy-test");
    {
        ClipboardStore store;
        CHECK(store.Available());
        CHECK(store.Save(h.Items(), 1));
    }
    {
        ClipboardStore store;
        auto loaded = store.Load(1);
        CHECK(loaded.size() == 3);
        CHECK(loaded[0].pinned && loaded[1].pinned && !loaded[2].pinned);
        CHECK(loaded[0].data == "image" && loaded[1].text == "pinned");
        std::reverse(loaded.begin(), loaded.end());
        h.ReplaceItems(std::move(loaded));
        CHECK(h.Items()[0].pinned && h.Items()[1].pinned);
        CHECK(h.TogglePinned(0));
        CHECK(h.Items().size() == 2 && h.Items()[0].pinned && !h.Items()[1].pinned);
        CHECK(!h.TogglePinned(100));
    }
    return 0;
}
