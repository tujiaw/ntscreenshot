#include "modules/local_search/LocalSearchModule.h"

#include "core/settings/SettingModel.h"
#include "core/platform/Util.h"
#include "modules/local_search/application/SearchIndexService.h"
#include "modules/local_search/application/SearchProviders.h"
#include "modules/local_search/infrastructure/SearchIndexStore.h"
#include "modules/local_search/ui/LocalSearchWidget.h"

#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>

namespace {

void applyWebSearchEngine(LocalSearchWidget* widget, const QString& engineId)
{
    const auto engines = SettingModel::localSearchWebEngines();
    for (const auto& engine : engines) {
        if (engine.id == engineId) {
            widget->setWebSearchEngine(engine.baseUrl, engine.queryParam);
            return;
        }
    }
}

} // namespace

LocalSearchModule::LocalSearchModule(SettingModel* settings)
    : settings_(settings)
{
    qInfo() << "LocalSearchModule: created";
}

void LocalSearchModule::initialize()
{
    qInfo() << "LocalSearchModule::initialize: start";
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    qInfo() << "LocalSearchModule::initialize: dataDir =" << dataDir;
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + QStringLiteral("/local_search.db");
    qInfo() << "LocalSearchModule::initialize: opening database at" << dbPath;
    store_ = std::make_shared<SearchIndexStore>(dbPath);
    QString error;
    if (!store_->initialize(&error)) {
        qWarning() << "Local search database:" << error;
    } else {
        qInfo() << "LocalSearchModule::initialize: database opened, item count =" << store_->itemCount();
    }
    qInfo() << "LocalSearchModule::initialize: creating SearchAggregator...";
    aggregator_ = std::make_shared<SearchAggregator>(store_);
    qInfo() << "LocalSearchModule::initialize: creating SearchIndexService...";
    service_ = new SearchIndexService(store_, this);
    qInfo() << "LocalSearchModule::initialize: applying settings (roots, exclude patterns, bookmarks)...";
    applySettings();
    qInfo() << "LocalSearchModule::initialize: done";
}

void LocalSearchModule::shutdown()
{
    qInfo() << "LocalSearchModule::shutdown: start";
    if (window_) {
        qInfo() << "LocalSearchModule::shutdown: closing search window";
        QWidget* window = window_;
        window_.clear();
        widget_.clear();
        window->close();
        window->deleteLater();
    }
    delete service_;
    service_ = nullptr;
    aggregator_.reset();
    store_.reset();
    qInfo() << "LocalSearchModule::shutdown: done";
}

void LocalSearchModule::open()
{
    qInfo() << "LocalSearchModule::open: start";
    if (!service_) {
        qInfo() << "LocalSearchModule::open: service not initialized, initializing...";
        initialize();
    }
    if (!window_) {
        qInfo() << "LocalSearchModule::open: creating LocalSearchWidget...";
        auto* widget = new LocalSearchWidget(aggregator_, store_, service_);
        window_ = widget;
        widget_ = widget;
        connect(widget, &LocalSearchWidget::requestHide, widget, &QWidget::hide);
        connect(widget, &LocalSearchWidget::preferredHeightChanged,
                this, &LocalSearchModule::updateWindowGeometry);
        connect(widget, &LocalSearchWidget::requestSetWebSearchEngine, this,
                [this](const QString& engineId) {
                    if (!settings_) return;
                    settings_->setLocalSearchWebEngine(engineId);
                    if (widget_) applyWebSearchEngine(widget_.data(), engineId);
                });
        qInfo() << "LocalSearchModule::open: LocalSearchWidget created";
    }
    applyWebSearchEngine(widget_.data(), settings_->localSearchWebEngine());
    widget_->setPinyinSearchEnabled(settings_->localSearchPinyinEnabled());
    widget_->setUpdatesEnabled(false);
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        anchorArea_ = screen->availableGeometry();
        widget_->setScaleFactor(Util::getScreenScaleFactor(anchorArea_.center()));
        widget_->setAvailableWidth(anchorArea_.width());
        // 立即把实际尺寸设为目标尺寸，确保 width() 与几何锚点计算一致，
        // 避免窗口在 show 后被 setFixedWidth 强制改宽导致"位置偏移"。
        widget_->resize(widget_->compactWindowSize());
        const QSize compactSize = widget_->compactWindowSize();
        const int x = anchorArea_.left() + (anchorArea_.width() - compactSize.width()) / 2;
        const int centerY = anchorArea_.top() + anchorArea_.height() / 3;
        anchorTopLeft_ = QPoint(x, centerY - compactSize.height() / 2);
    } else {
        anchorArea_ = QRect();
        anchorTopLeft_ = window_->pos();
    }
    widget_->activateSearch();
    updateWindowGeometry(widget_->compactWindowSize().height());
    widget_->setUpdatesEnabled(true);
    window_->show();
    window_->raise();
    window_->activateWindow();
    qInfo() << "LocalSearchModule::open: window shown";
}

void LocalSearchModule::updateWindowGeometry(int preferredHeight)
{
    if (!widget_) return;
    const QSize compactSize = widget_->compactWindowSize();
    int height = qMax(compactSize.height(), preferredHeight);
    if (!anchorArea_.isEmpty()) {
        const int availableHeight = anchorArea_.bottom() + 1 - anchorTopLeft_.y();
        height = qMax(compactSize.height(), qMin(height, availableHeight));
    }
    // 宽度始终用当前实际宽度，只改变高度，绝不改宽度 → 窗口水平位置永不动
    widget_->setGeometry(anchorTopLeft_.x(), anchorTopLeft_.y(), widget_->width(), height);
}

void LocalSearchModule::applySettings()
{
    qInfo() << "LocalSearchModule::applySettings: roots count =" << settings_->localSearchRoots().size()
            << "exclude patterns =" << settings_->localSearchExcludePatterns().size()
            << "bookmark sources =" << settings_->localSearchBookmarkSources();
    if (service_) {
        service_->applySettings(settings_->localSearchRoots(),
                                settings_->localSearchExcludePatterns(),
                                settings_->localSearchBookmarkSources());
    }
    if (widget_) applyWebSearchEngine(widget_.data(), settings_->localSearchWebEngine());
}

void LocalSearchModule::rebuildIndex()
{
    qInfo() << "LocalSearchModule::rebuildIndex: rebuilding all...";
    if (service_) {
        service_->rebuildAll();
        qInfo() << "LocalSearchModule::rebuildIndex: rebuild triggered";
    } else {
        qWarning() << "LocalSearchModule::rebuildIndex: no service";
    }
}

QString LocalSearchModule::statusText() const
{
    return service_ ? service_->statusText() : QStringLiteral("未初始化");
}
