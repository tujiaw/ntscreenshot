#include "modules/clipboard/HistoryWindow.h"
#include "modules/clipboard/FavoriteSyncController.h"
#include "modules/clipboard/FavoriteSyncPage.h"
#include <QApplication>
#include <QKeyEvent>
#include <QButtonGroup>
#include <QDir>
#include <QMenu>
#include <QTimer>
#include <QTemporaryDir>
#include <QPushButton>
#include <QFontDatabase>
#include <QElapsedTimer>
#include <QBuffer>
#include "core/theme/ThemeManager.h"
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int runFavoriteSyncControllerTest(QApplication& app, const QString& previewDir);

int runClipboardSwitchBenchmark(QApplication& app) {
    ClipboardHistory history;
    history.SetMaxItems(100);
    std::vector<ClipItem> items;
    for (int i = 0; i < 64; ++i) {
        ClipItem item;
        item.favorite = i < 32;
        item.capturedAt = QDateTime::currentDateTime();
        if (i % 32 < 12) {
            item.kind = ClipKind::Image;
            QImage image(800, 600, QImage::Format_ARGB32);
            image.fill(QColor::fromHsv(i * 5, 180, 200));
            QBuffer buffer(&item.data);
            buffer.open(QIODevice::WriteOnly);
            CHECK(image.save(&buffer, "PNG"));
            item.width = image.width(); item.height = image.height();
            // Legacy images intentionally have no pixel hash.
        } else {
            item.kind = ClipKind::Text;
            item.text = QString::number(i) + QString(65536, QLatin1Char('x'));
        }
        items.push_back(std::move(item));
    }
    history.ReplaceItems(std::move(items));
    HistoryWindow window(1.0);
    window.Configure(&history, {}, {}, {}, {}, {}, {}, false);
    window.Refresh(); window.show(); app.processEvents();
    auto* views = window.findChild<QButtonGroup*>(QStringLiteral("clipViewGroup"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("clipList"));
    QElapsedTimer timer;
    qint64 total = 0, slowest = 0;
    for (int i = 0; i < 8; ++i) {
        timer.start();
        views->button(i % 2 == 0 ? 1 : 0)->click();
        app.processEvents();
        const auto elapsed = timer.nsecsElapsed();
        total += elapsed; slowest = qMax(slowest, elapsed);
        CHECK(list->count() == 32);
    }
    std::fprintf(stderr, "Clipboard benchmark: 64 items, 24 legacy PNGs, 40 long texts; 8 switches average %.2f ms, maximum %.2f ms\n", total / 8e6, slowest / 1e6);
    return 0;
}

int runClipboardCacheRegression(QApplication& app) {
    const auto imageItem = [](QColor color, bool favorite, int quality) {
        ClipItem item;
        item.kind = ClipKind::Image; item.favorite = favorite;
        item.capturedAt = QDateTime::currentDateTime();
        item.width = 64; item.height = 64;
        QImage image(64, 64, QImage::Format_ARGB32); image.fill(color);
        QBuffer buffer(&item.data); buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG", quality);
        return item;
    };
    auto favorite = imageItem(Qt::red, true, 0);
    auto samePixels = imageItem(Qt::red, false, 100);
    auto other = imageItem(Qt::blue, false, 100);
    CHECK(favorite.data != samePixels.data);
    ClipboardHistory history;
    history.ReplaceItems({favorite, samePixels, other});
    HistoryWindow window(1.0);
    window.Configure(&history, {}, {}, {}, {}, {}, {}, false);
    window.Refresh(); window.show(); app.processEvents();
    auto* list = window.findChild<QListWidget*>(QStringLiteral("clipList"));
    auto* views = window.findChild<QButtonGroup*>(QStringLiteral("clipViewGroup"));
    CHECK(list->count() == 2 && list->item(0)->data(Qt::UserRole + 1).toBool());
    CHECK(!list->item(1)->data(Qt::UserRole + 1).toBool());
    CHECK(list->viewport()->grab().toImage().pixelColor(40, 20) == QColor(Qt::red));
    // Reorder indices after caching, then change an existing favorite's pixels.
    history.ReplaceItems({favorite, other, samePixels}); window.Refresh(); app.processEvents();
    CHECK(!list->item(0)->data(Qt::UserRole + 1).toBool());
    CHECK(list->item(1)->data(Qt::UserRole + 1).toBool());
    CHECK(list->viewport()->grab().toImage().pixelColor(40, 20) == QColor(Qt::blue));
    history.ReplaceItems({imageItem(Qt::blue, true, 0), other, samePixels}); window.Refresh();
    CHECK(list->item(0)->data(Qt::UserRole + 1).toBool());
    CHECK(!list->item(1)->data(Qt::UserRole + 1).toBool());
    views->button(1)->click(); views->button(0)->click();
    CHECK(list->item(0)->data(Qt::UserRole + 1).toBool());
    history.ReplaceItems({}); window.Refresh(); CHECK(list->count() == 0);
    history.AddText(QString(100000, QLatin1Char('x')) + QStringLiteral(" searchable-tail")); window.Refresh();
    window.findChild<QLineEdit*>(QStringLiteral("clipSearchEdit"))->setText(QStringLiteral("searchable-tail"));
    CHECK(list->count() == 1); // Preview truncation must not truncate full-text search.
    return 0;
}

int runClipboardWindowTest(QApplication& app) {
    // The offscreen plugin does not enumerate installed Windows fonts reliably.
    // Load the same UI font explicitly so Chinese preview QA is representative.
    const QString fontDirectory = qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/");
    QFontDatabase::addApplicationFont(fontDirectory + QStringLiteral("msyh.ttc"));
    QFontDatabase::addApplicationFont(fontDirectory + QStringLiteral("msyhbd.ttc"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
    ThemeManager::apply(AppTheme::Dark);
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_CLIPBOARD_BENCHMARK")) return runClipboardSwitchBenchmark(app);
    ClipboardHistory history;
    history.AddText(QStringLiteral("favorite shared"));
    history.ToggleFavorite(0);
    history.AddText(QStringLiteral("ordinary shared"));
    history.AddText(QStringLiteral("another entry"));
    HistoryWindow window(1.0);
    size_t pasted = size_t(-1);
    window.Configure(&history, [&](size_t index) { pasted = index; },
        [&](size_t index) { history.RemoveAt(index); window.Refresh(); }, {}, {}, {}, {}, false,
        [&](size_t index) { history.ToggleFavorite(index); window.Refresh(); });
    window.Refresh();
    auto* views = window.findChild<QButtonGroup*>(QStringLiteral("clipViewGroup"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("clipList"));
    auto* search = window.findChild<QLineEdit*>(QStringLiteral("clipSearchEdit"));
    CHECK(views && list && search);
    auto* syncBar = window.findChild<QWidget*>(QStringLiteral("clipSyncBar"));
    auto* syncButton = window.findChild<QToolButton*>(QStringLiteral("clipSyncSettingsButton"));
    CHECK(!syncBar && syncButton && !syncButton->icon().isNull());
    auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("clipPages"));
    CHECK(pages && pages->count() == 2);
    syncButton->click();
    CHECK(views->checkedId() == 2 && pages->currentIndex() == 1 && !search->isEnabled());
    CHECK(!syncButton->toolTip().isEmpty());
    QKeyEvent syncEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    app.sendEvent(syncButton->parentWidget(), &syncEnter);
    CHECK(pasted == size_t(-1));
    views->button(0)->click();
    CHECK(pages->currentIndex() == 0 && search->isEnabled());
    CHECK(views->buttons().size() == 3 && views->exclusive() && views->checkedId() == 0);
    CHECK(views->button(0)->parentWidget()->parentWidget() == search->parentWidget());
    CHECK(views->button(1)->parentWidget()->parentWidget() == search->parentWidget());
    CHECK(!views->button(0)->icon().isNull() && !views->button(1)->icon().isNull());
    CHECK(list->count() == 3);
    CHECK(list->item(2)->data(Qt::UserRole + 1).toBool());
    const QString previewDir = qEnvironmentVariable("NTSCREENSHOT_CLIPBOARD_PREVIEW_DIR");
    if (!previewDir.isEmpty()) {
        CHECK(QDir().mkpath(previewDir));
        window.show();
        app.processEvents();
        CHECK(window.grab().save(previewDir + QStringLiteral("/clipboard-marked.png")));
    }
    CHECK(window.HistoryIndexForRow(-1) == size_t(-1));
    CHECK(window.HistoryIndexForRow(3) == size_t(-1));
    search->setText(QStringLiteral("shared"));
    CHECK(list->count() == 2 && list->item(0)->text() == QStringLiteral("ordinary shared"));
    views->button(1)->click();
    CHECK(views->checkedId() == 1);
    CHECK(!window.findChild<QWidget*>(QStringLiteral("clipSyncBar")));
    CHECK(list->count() == 1 && list->item(0)->text() == QStringLiteral("favorite shared"));
    CHECK(!list->item(0)->data(Qt::UserRole + 1).toBool());
    if (!previewDir.isEmpty()) {
        CHECK(QDir().mkpath(previewDir));
        window.show();
        app.processEvents();
        CHECK(window.grab().save(previewDir + QStringLiteral("/favorites.png")));
    }
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    app.sendEvent(list, &enter);
    CHECK(pasted < history.Items().size() && history.Items()[pasted].text == QStringLiteral("favorite shared"));
    search->setText(QStringLiteral("ordinary"));
    CHECK(list->count() == 0);
    views->button(0)->click();
    CHECK(views->checkedId() == 0);
    CHECK(list->count() == 1);
    search->clear();
    CHECK(list->count() == 3);
    views->button(1)->click();
    CHECK(views->checkedId() == 1);
    CHECK(list->count() == 1);
    history.ToggleFavorite(size_t(list->item(0)->data(Qt::UserRole).toInt()));
    window.Refresh();
    CHECK(list->count() == 0);
    views->button(0)->click();
    CHECK(views->checkedId() == 0);
    CHECK(list->count() == 3);
    for (int row = 0; row < list->count(); ++row) CHECK(!list->item(row)->data(Qt::UserRole + 1).toBool());
    if (!previewDir.isEmpty()) {
        app.processEvents();
        CHECK(window.grab().save(previewDir + QStringLiteral("/clipboard.png")));
    }
    // Image entries participate in the same tabs and retain their source index.
    history.AddImage(QByteArray("image"), 1, 1, 42);
    history.ToggleFavorite(0);
    window.Refresh();
    CHECK(list->count() == 4);
    views->button(1)->click();
    CHECK(views->checkedId() == 1);
    CHECK(list->count() == 1);
    CHECK(history.Items()[window.HistoryIndexForRow(0)].kind == ClipKind::Image);
    QKeyEvent remove(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    app.sendEvent(list, &remove);
    CHECK(list->count() == 0 && history.Items().size() == 4);
    views->button(0)->click();
    CHECK(views->checkedId() == 0);
    window.show();
    app.processEvents();
    for (int attempt = 0; attempt < 2; ++attempt) {
        bool sawFavoriteAction = false;
        QTimer::singleShot(0, &window, [&]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            for (auto* action : menu->actions()) {
                if (action->text() == QCoreApplication::translate("App", "收藏")) {
                    sawFavoriteAction = true;
                    menu->setActiveAction(action);
                    QKeyEvent choose(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    app.sendEvent(menu, &choose);
                    return;
                }
            }
            menu->close();
        });
        list->customContextMenuRequested(list->visualItemRect(list->item(0)).center());
        CHECK(sawFavoriteAction && list->count() == 4);
    }
    views->button(1)->click();
    CHECK(views->checkedId() == 1);
    CHECK(list->count() == 1 && history.Items()[window.HistoryIndexForRow(0)].favorite);
    window.resize(HistoryWindow::kMinimumWidth, HistoryWindow::kMinimumHeight);
    app.processEvents();
    auto* switcher = views->button(0)->parentWidget();
    CHECK(search->geometry().right() < switcher->geometry().left());
    auto* closeButton = window.findChild<QToolButton*>(QStringLiteral("clipCloseButton"));
    CHECK(closeButton && syncButton->geometry().right() < closeButton->geometry().left());
    CHECK(list->mapTo(&window, QPoint(0, 0)).y() < 50);
    // Hosted sync is ready to sign in without service setup or extra checkboxes.
    QTemporaryDir syncStorage;
    CHECK(syncStorage.isValid());
    const QByteArray originalLocalAppData = qgetenv("LOCALAPPDATA");
    qputenv("LOCALAPPDATA", syncStorage.path().toLocal8Bit());
    {
        ClipboardStore syncStore;
        CHECK(syncStore.Available());
        ClipboardHistory syncHistory;
        FavoriteSyncController controller(syncHistory, syncStore);
        controller.Initialize();
        CHECK(!controller.SignedIn() && !controller.SigningIn());
        CHECK(controller.Endpoint().startsWith(QStringLiteral("https://")));
        auto* page = new FavoriteSyncPage(&controller);
        window.SetSyncPage(page);
        auto& dialog = *page;
        CHECK(!dialog.isWindow());
        window.ShowSyncPage();
        CHECK(views->checkedId() == 2 && pages->currentWidget() == page);
        auto* signIn = dialog.findChild<QPushButton*>(QStringLiteral("favoriteSyncSignIn"));
        CHECK(signIn && signIn->isEnabled());
        CHECK(!dialog.findChild<QLineEdit*>(QStringLiteral("favoriteSyncEndpoint")));
        auto* syncNow = dialog.findChild<QPushButton*>(QStringLiteral("favoriteSyncNow"));
        CHECK(syncNow && syncNow->isHidden());
        if (!previewDir.isEmpty()) {
            window.resize(340, 460);
            window.show();
            app.processEvents();
            CHECK(window.grab().save(previewDir + QStringLiteral("/sync-dark.png")));
            ThemeManager::apply(AppTheme::Light);
            app.processEvents();
            CHECK(window.grab().save(previewDir + QStringLiteral("/sync-light.png")));
            window.resize(HistoryWindow::kMinimumWidth, HistoryWindow::kMinimumHeight);
            app.processEvents();
            CHECK(page->width() <= window.width());
            CHECK(signIn->width() < page->width());
            CHECK(window.grab().save(previewDir + QStringLiteral("/sync-small.png")));
        }
        window.SetSyncPage(new QWidget);
    }
    qputenv("LOCALAPPDATA", originalLocalAppData);
    CHECK(runClipboardCacheRegression(app) == 0);
    return runFavoriteSyncControllerTest(app, previewDir);
}
