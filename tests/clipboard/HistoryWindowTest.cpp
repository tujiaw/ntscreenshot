#include "modules/clipboard/HistoryWindow.h"
#include <QApplication>
#include <QKeyEvent>
#include <QButtonGroup>
#include <QDir>
#include <QMenu>
#include <QTimer>
#include "core/theme/ThemeManager.h"
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int runClipboardWindowTest(QApplication& app) {
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
    ThemeManager::apply(AppTheme::Dark);
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
    CHECK(views->buttons().size() == 2 && views->exclusive() && views->checkedId() == 0);
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
    CHECK(list->mapTo(&window, QPoint(0, 0)).y() < 50);
    return 0;
}
