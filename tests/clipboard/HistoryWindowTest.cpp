#include "modules/clipboard/HistoryWindow.h"
#include "modules/clipboard/FavoriteSyncController.h"
#include "modules/clipboard/FavoriteSyncPage.h"
#include "modules/clipboard/FavoriteAccountParser.h"
#include <QApplication>
#include <QClipboard>
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
#include <QScrollBar>
#include <QPlainTextEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QEventLoop>
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
    window.findChild<QLineEdit*>(QStringLiteral("clipSearchEdit"))->clear();
    history.ReplaceItems({});
    for (int i = 0; i < 40; ++i) history.AddText(QStringLiteral("entry %1").arg(i));
    window.Configure(&history, {}, {}, {}, {}, {}, {}, false,
        [&](size_t index) { history.ToggleFavorite(index); window.Refresh(); });
    window.Refresh();
    list->setCurrentRow(25);
    list->scrollToItem(list->currentItem(), QAbstractItemView::PositionAtCenter);
    app.processEvents();
    const QString selectedText = list->currentItem()->text();
    const int scrollPosition = list->verticalScrollBar()->value();
    CHECK(scrollPosition > 0);
    for (int i = 0; i < 4; ++i) {
        QKeyEvent favoriteKey(QEvent::KeyPress, Qt::Key_D, Qt::ControlModifier);
        app.sendEvent(list, &favoriteKey); app.processEvents();
        CHECK(list->count() == 40 && list->currentRow() == 25);
        CHECK(list->currentItem()->text() == selectedText);
        CHECK(list->currentItem()->data(Qt::UserRole + 1).toBool() == (i % 2 == 0));
        CHECK(list->verticalScrollBar()->value() == scrollPosition);
    }
    return 0;
}

int runClipboardPreviewRegression(QApplication& app, const QString& previewDir) {
    ClipItem first; first.text = QStringLiteral("<b>plain text</b>\n") + QString(10000, QLatin1Char('x'));
    ClipItem second; second.kind = ClipKind::Image;
    QImage image(320, 200, QImage::Format_ARGB32); image.fill(Qt::green);
    QBuffer buffer(&second.data); buffer.open(QIODevice::WriteOnly); CHECK(image.save(&buffer, "PNG"));
    ClipItem third; third.text = QStringLiteral("another selected item");
    ClipboardHistory history; history.ReplaceItems({first, second, third});
    HistoryWindow window(1.0); window.Configure(&history, {}, {}, {}, {}, {}, {}, false);
    window.Refresh(); window.show(); window.activateWindow(); app.processEvents();
    auto* list = window.findChild<QListWidget*>(QStringLiteral("clipList"));
    QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
    app.sendEvent(list, &space); app.processEvents();
    auto* preview = window.findChild<QWidget*>(QStringLiteral("clipboardPreviewWindow"));
    CHECK(preview && preview->isWindow() && preview->isVisible());
    auto* text = preview->findChild<QPlainTextEdit*>(QStringLiteral("clipboardPreviewText"));
    auto* canvas = preview->findChild<QGraphicsView*>(QStringLiteral("clipboardPreviewImage"));
    CHECK(text && canvas && text->isReadOnly() && text->toPlainText() == first.text);
    if (!previewDir.isEmpty()) CHECK(preview->grab().save(previewDir + QStringLiteral("/preview-text.png")));
    list->setCurrentRow(1); app.processEvents();
    CHECK(canvas->isVisible() && !text->isVisible() && canvas->scene()->items().size() == 1);
    if (!previewDir.isEmpty()) CHECK(preview->grab().save(previewDir + QStringLiteral("/preview-image.png")));
    QKeyEvent next(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    app.sendEvent(canvas->viewport(), &next); app.processEvents();
    CHECK(list->currentRow() == 2 && text->isVisible() && text->toPlainText() == third.text);
    app.sendEvent(list, &space); app.processEvents();
    CHECK(window.findChildren<QWidget*>(QStringLiteral("clipboardPreviewWindow")).size() == 1);
    const auto settleFocus = [&]() { QEventLoop loop; QTimer::singleShot(80, &loop, &QEventLoop::quit); loop.exec(); };
    preview->activateWindow(); app.processEvents(); settleFocus(); CHECK(preview->isVisible());
    window.activateWindow(); app.processEvents(); settleFocus(); CHECK(preview->isVisible());
    QWidget outside; outside.show(); outside.activateWindow(); app.processEvents(); settleFocus();
    CHECK(!preview->isVisible()); outside.hide();
    window.activateWindow(); app.processEvents(); app.sendEvent(list, &space); app.processEvents();
    CHECK(preview->isVisible());
    QEvent deactivate(QEvent::ApplicationDeactivate); app.sendEvent(&app, &deactivate);
    CHECK(!preview->isVisible());
    app.sendEvent(list, &space); app.processEvents();
    CHECK(preview->isVisible() && window.findChild<QWidget*>(QStringLiteral("clipboardPreviewWindow")) == preview);
    history.ReplaceItems({}); window.Refresh(); CHECK(!preview->isVisible());
    history.AddText("reopened"); window.Refresh(); app.sendEvent(list, &space); app.processEvents();
    CHECK(preview->isVisible() && text->toPlainText() == QStringLiteral("reopened"));
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    app.sendEvent(text, &escape); CHECK(!preview->isVisible() && window.isVisible());
    app.sendEvent(list, &space); window.hide(); CHECK(!preview->isVisible());
    return 0;
}

int runFavoriteAccountRegression(QApplication& app) {
    struct Example { const char* input; const char* username; const char* password; const char* url = ""; };
    const Example examples[] = {
        {"用户名：xyz\n密码：dfdf", "xyz", "dfdf"},
        {"username: admin password: 123", "admin", "123"},
        {"账户：test \n密码: 123", "test", "123"},
        {"user: test\npassword: 123", "test", "123"},
        {"网址等信息\n登录：test\n密码：123", "test", "123"},
        {"USERNAME = admin; PASSWORD = a:b=c!@#$%", "admin", "a:b=c!@#$%"},
        {"帐号: test，口令: 123", "test", "123"},
        {"帐户：test | pwd：123", "test", "123"},
        {"user_name：admin\r\npasswd＝123", "admin", "123"},
        {"login id: admin\npass word: 123", "admin", "123"},
        {"邮箱：a@example.com\n登录密码：123", "a@example.com", "123"},
        {"用户名\tadmin\n密码\t123", "admin", "123"},
        {"网址: https://example.com 用户名: admin 密码: 123 备注: demo", "admin", "123", "https://example.com"},
        {"用户名: admin\n密码: '  a; password: b  '", "admin", "  a; password: b  "},
        {"用户名: admin\n密码: p:a=s;#%", "admin", "p:a=s;#%"},
        {"{\"username\": \"admin\", \"password\": \"123\"}", "admin", "123"},
        {"用户名：test", "test", ""},
        {"密码：123", "", "123"},
        {"password: username: test", "test", ""},
        {"用户名：\n密码：", "", ""},
        {"https://example.com/?username=admin&password=123", "", "", "https://example.com/?username=admin&password=123"},
        {"用户名册：test\n修改密码：123\nusernames: test", "", ""},
        {"https://example.com/login\n用户名: admin\n密码: 123", "admin", "123", "https://example.com/login"},
        {"网站: example.com/login", "", "", "example.com/login"},
        {"URL = https://example.com/?a=1&b=%2Fhome#top", "", "", "https://example.com/?a=1&b=%2Fhome#top"},
        {"登录地址：'https://example.com/a(b)'", "", "", "https://example.com/a(b)"},
        {"访问 (https://example.com/login).", "", "", "https://example.com/login"},
        {"请访问 https://example.com/login。", "", "", "https://example.com/login"},
        {"www.example.com/login", "", "", "www.example.com/login"},
        {"网址: http://localhost:8080/login", "", "", "http://localhost:8080/login"},
        {"网址: http://intranet/login", "", "", "http://intranet/login"},
        {"地址: 127.0.0.1:8080", "", "", "127.0.0.1:8080"},
        {"http://[::1]:8080/login", "", "", "http://[::1]:8080/login"},
        {"网址: https://example.com\n备注: https://docs.example.com", "", "", "https://example.com"},
        {"网址: https://one.example.com\n网址: https://two.example.com", "", "", ""},
        {"https://one.example.com\nhttps://two.example.com", "", "", ""},
        {"网址: https://example.com\nURL: https://example.com", "", "", "https://example.com"},
        {"https://example.com/?a=1;user=admin;password=123", "", "", "https://example.com/?a=1;user=admin;password=123"},
        {"用户名: admin\n密码: https://example.com/secret", "admin", "https://example.com/secret", ""},
        {"密码: 'abc https://example.com secret'", "", "abc https://example.com secret", ""},
        {"网址: javascript:alert(1)", "", "", ""},
        {"网址: https:///missing-host", "", "", ""},
        {"网址: 不是网址", "", "", ""},
        {"test / 123", "", ""},
        {"user: first password: one\nuser: second password: two", "", ""},
    };
    int exampleIndex = 0;
    for (const auto& example : examples) {
        ++exampleIndex;
        const auto fields = ParseFavoriteAccount(QString::fromUtf8(example.input));
        if (fields.username != QString::fromUtf8(example.username) || fields.password != QString::fromUtf8(example.password) || fields.url != QString::fromUtf8(example.url))
            std::fprintf(stderr, "Account format fixture %d failed\n", exampleIndex);
        CHECK(fields.username == QString::fromUtf8(example.username));
        CHECK(fields.password == QString::fromUtf8(example.password));
        CHECK(fields.url == QString::fromUtf8(example.url));
    }
    ClipboardHistory history; history.AddText(QStringLiteral("username: admin password: 123 网址: https://example.com/login?next=%2Fhome&a=1")); history.ToggleFavorite(0);
    HistoryWindow window(1.0); window.Configure(&history, {}, {}, {}, {}, {}, {}, false); window.Refresh();
    auto* views = window.findChild<QButtonGroup*>(QStringLiteral("clipViewGroup"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("clipList"));
    window.show(); app.processEvents();
    const auto inspectMenu = [&](bool fieldsExpected, const char* choose) {
        bool valid = false;
        QTimer::singleShot(0, &window, [&]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            QAction *username = nullptr, *password = nullptr, *url = nullptr;
            for (auto* action : menu->actions()) {
                if (action->text() == QCoreApplication::translate("App", "复制用户名")) username = action;
                if (action->text() == QCoreApplication::translate("App", "复制密码")) password = action;
                if (action->text() == QCoreApplication::translate("App", "复制网址")) url = action;
            }
            valid = bool(username) == fieldsExpected && bool(password) == fieldsExpected && bool(url) == fieldsExpected;
            auto* selected = choose ? (QString::fromUtf8(choose) == QStringLiteral("username") ? username : QString::fromUtf8(choose) == QStringLiteral("password") ? password : url) : nullptr;
            if (selected) {
                menu->setActiveAction(selected);
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); app.sendEvent(menu, &enter);
            } else menu->close();
        });
        list->customContextMenuRequested(list->visualItemRect(list->item(0)).center());
        return valid;
    };
    CHECK(inspectMenu(false, nullptr)); // A starred ordinary history row must not expose the fields.
    views->button(1)->click();
    CHECK(inspectMenu(true, "username")); CHECK(QApplication::clipboard()->text() == QStringLiteral("admin"));
    CHECK(inspectMenu(true, "password")); CHECK(QApplication::clipboard()->text() == QStringLiteral("123"));
    CHECK(inspectMenu(true, "url")); CHECK(QApplication::clipboard()->text() == QStringLiteral("https://example.com/login?next=%2Fhome&a=1"));
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
    CHECK(syncButton->toolTip().isEmpty());
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
    for (int attempt = 0; attempt < 3; ++attempt) {
        bool sawFavoriteAction = false;
        QTimer::singleShot(0, &window, [&]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            for (auto* action : menu->actions()) {
                if (action->text() == QCoreApplication::translate("App", attempt == 1 ? "取消收藏" : "收藏")) {
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
    CHECK(runFavoriteAccountRegression(app) == 0);
    CHECK(runClipboardCacheRegression(app) == 0);
    CHECK(runClipboardPreviewRegression(app, previewDir) == 0);
    return runFavoriteSyncControllerTest(app, previewDir);
}
