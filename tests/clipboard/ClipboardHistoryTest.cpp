#include "modules/clipboard/ClipboardHistory.h"
#include "modules/clipboard/ClipboardStore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QImage>
#include <QBuffer>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ClipboardHistory independent;
    independent.AddText("shared content");
    CHECK(independent.ToggleFavorite(0));
    CHECK(independent.Items().size() == 2);
    CHECK(independent.Items()[0].favorite && !independent.Items()[1].favorite);
    CHECK(independent.ToggleFavorite(1));
    CHECK(independent.Items().size() == 2); // Repeated favorite is idempotent.
    independent.Clear();
    CHECK(independent.Items().size() == 1);
    CHECK(independent.AddText("shared content"));
    CHECK(independent.Items().size() == 2 && !independent.Items()[1].favorite);
    CHECK(independent.RemoveAt(1));
    CHECK(independent.Items().size() == 1 && independent.Items()[0].favorite);
    independent.AddImage(QByteArray("same png"), 1, 1, 123);
    CHECK(independent.ToggleFavorite(1));
    independent.Clear();
    independent.AddImage(QByteArray("same png"), 1, 1, 123);
    CHECK(independent.Items().size() == 3);
    CHECK(independent.ToggleFavorite(2));
    CHECK(independent.Items().size() == 3);
    auto earlier = independent.Items();
    const QDateTime oldTime = QDateTime::fromMSecsSinceEpoch(1000);
    for (auto& item : earlier) item.capturedAt = oldTime;
    independent.ReplaceItems(earlier);
    independent.AddText("shared content");
    CHECK(independent.Items().size() == 4);
    CHECK(independent.Items()[1].favorite && independent.Items()[1].capturedAt == oldTime);
    independent.AddText("shared content");
    CHECK(independent.Items().size() == 4);
    independent.AddImage(QByteArray("same png"), 1, 1, 123);
    CHECK(independent.Items().size() == 4);
    CHECK(independent.Items()[0].capturedAt == oldTime);
    CHECK(independent.Items()[1].capturedAt == oldTime);
    CHECK(independent.ToggleFavorite(2)); // Re-favorite the ordinary image.
    CHECK(independent.Items().size() == 4);
    CHECK(independent.Items()[0].capturedAt > oldTime);
    CHECK(independent.Items()[1].capturedAt == oldTime);
    CHECK(independent.ToggleFavorite(3)); // Re-favorite the ordinary text.
    CHECK(independent.Items().size() == 4);
    CHECK(independent.Items()[1].capturedAt > oldTime);
    ClipboardHistory firstFavorite;
    ClipItem oldCopy;
    oldCopy.text = "copied earlier";
    oldCopy.capturedAt = oldTime;
    firstFavorite.ReplaceItems({oldCopy});
    CHECK(firstFavorite.ToggleFavorite(0));
    CHECK(firstFavorite.Items()[0].capturedAt > oldTime);
    CHECK(firstFavorite.Items()[1].capturedAt == oldTime);
    QImage pixels(2, 2, QImage::Format_ARGB32);
    pixels.fill(Qt::red);
    QByteArray firstPng, secondPng;
    QBuffer firstBuffer(&firstPng), secondBuffer(&secondPng);
    CHECK(pixels.save(&firstBuffer, "PNG"));
    pixels.setText("note", "different metadata, identical pixels");
    CHECK(pixels.save(&secondBuffer, "PNG"));
    CHECK(firstPng != secondPng);
    ClipboardHistory legacyImage;
    legacyImage.AddImage(firstPng, 2, 2); // Old database images had no pixel hash.
    legacyImage.ToggleFavorite(0);
    legacyImage.Clear();
    legacyImage.AddImage(secondPng, 2, 2, 456);
    legacyImage.ToggleFavorite(1);
    CHECK(legacyImage.Items().size() == 2);
    // Equal pixel streams can have different dimensions and must remain distinct.
    QImage narrow(1, 4, QImage::Format_ARGB32);
    narrow.fill(Qt::red);
    QByteArray narrowPng;
    QBuffer narrowBuffer(&narrowPng);
    CHECK(narrow.save(&narrowBuffer, "PNG"));
    legacyImage.AddImage(narrowPng, 1, 4, 456);
    legacyImage.ToggleFavorite(1);
    CHECK(legacyImage.Items().size() == 4);
    ClipboardHistory h;
    h.SetMaxItems(2);
    h.AddText("pinned");
    CHECK(h.ToggleFavorite(0));
    h.AddText("old"); h.AddText("recent"); h.AddText("new");
    CHECK(h.Items().size() == 3);
    ClipboardHistory cleared;
    cleared.ReplaceItems(h.Items());
    cleared.Clear();
    CHECK(cleared.Items().size() == 1 && cleared.Items()[0].favorite);
    CHECK(h.Items()[0].text == "pinned");
    h.AddText("recent");
    CHECK(h.Items()[0].favorite && h.Items()[1].text == "recent");
    h.AddImage(QByteArray("image"), 1, 1, 42);
    CHECK(h.ToggleFavorite(1));
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
        CHECK(loaded[0].favorite && loaded[1].favorite && !loaded[2].favorite);
        CHECK(loaded[0].data == "image" && loaded[1].text == "pinned");
        CHECK(loaded[0].pixelHash == 42);
        std::reverse(loaded.begin(), loaded.end());
        h.ReplaceItems(std::move(loaded));
        CHECK(h.Items()[0].favorite && h.Items()[1].favorite);
        CHECK(h.ToggleFavorite(0));
        CHECK(h.Items().size() == 2 && h.Items()[0].favorite && !h.Items()[1].favorite);
        CHECK(!h.ToggleFavorite(100));
    }
    // Favorites outlive history trimming and round-trip without a count limit.
    ClipboardHistory favorites;
    favorites.SetMaxItems(1);
    for (int i = 0; i < 250; ++i) {
        favorites.AddText(QStringLiteral("favorite %1").arg(i));
        CHECK(favorites.ToggleFavorite(favorites.Items().size() - 1));
    }
    favorites.AddText("history old");
    favorites.AddText("history latest");
    CHECK(favorites.Items().size() == 251);
    {
        ClipboardStore store;
        CHECK(store.Save(favorites.Items(), 1));
    }
    {
        ClipboardStore store;
        const auto loaded = store.Load(1);
        CHECK(loaded.size() == 251);
        CHECK(std::count_if(loaded.begin(), loaded.end(), [](const ClipItem& item) { return item.favorite; }) == 250);
        CHECK(loaded.back().text == "history latest");
        favorites.ReplaceItems(loaded);
        favorites.Clear();
        CHECK(favorites.Items().size() == 250);
        CHECK(store.Save(favorites.Items(), 1));
    }
    {
        ClipboardStore store;
        CHECK(store.Load(1).size() == 250);
    }
    return 0;
}
