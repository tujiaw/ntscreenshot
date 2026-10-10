#include "ImageBrowserWindow.h"
#include "ImageCanvas.h"
#include "ImageDirectoryModel.h"
#include "ImageLoader.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QDir>
#include <QFormLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMovie>
#include <QPainter>
#include <QPushButton>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <functional>

namespace ImageBrowser {
namespace {
class ToolbarStyle final : public QProxyStyle {
public:
    ToolbarStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}
    int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override {
        if (metric == PM_ToolBarExtensionExtent && widget && widget->property("browserExtensionExtent").isValid())
            return widget->property("browserExtensionExtent").toInt();
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

class ThumbnailDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    double scale = 1;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(qRound(144 * scale), qRound(112 * scale));
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto& tokens = ThemeManager::tokens();
        painter->save();
        painter->fillRect(option.rect, option.state & QStyle::State_Selected ? tokens.accentSubtle : tokens.surface);
        const int margin = qRound(8 * scale);
        const int labelHeight = qMax(qRound(24 * scale), option.fontMetrics.height() + qRound(4 * scale));
        const QRect imageRect = option.rect.adjusted(margin, qRound(6 * scale), -margin, -labelHeight - qRound(4 * scale));
        const QPixmap pixmap = qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));
        if (!pixmap.isNull()) {
            const QSize size = pixmap.size().scaled(imageRect.size(), Qt::KeepAspectRatio);
            painter->drawPixmap(QRect(imageRect.center() - QPoint(size.width()/2, size.height()/2), size), pixmap);
        } else {
            painter->setPen(tokens.textSecondary);
            painter->drawText(imageRect, Qt::AlignCenter, index.data(Qt::UserRole).toBool()
                ? QCoreApplication::translate("ImageBrowser::ImageBrowserWindow", "无法预览") : QStringLiteral("..."));
        }
        painter->setPen(tokens.textPrimary);
        const QRect textRect = option.rect.adjusted(margin, option.rect.height() - labelHeight, -margin, -qRound(4 * scale));
        painter->drawText(textRect, Qt::AlignCenter,
                          option.fontMetrics.elidedText(index.data().toString(), Qt::ElideMiddle, textRect.width()));
        painter->restore();
    }
};

class SaveProgress final : public QDialog {
public:
    explicit SaveProgress(QWidget* parent) : QDialog(parent) {
        setWindowTitle(QCoreApplication::translate("ImageBrowser::ImageBrowserWindow", "保存图片"));
        setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(QCoreApplication::translate("ImageBrowser::ImageBrowserWindow", "正在保存图片…"), this));
        setMinimumWidth(260);
    }
    void reject() override {}
};
}

ImageBrowserWindow::ImageBrowserWindow(SettingModel* settings, ImageLoader* loader)
    : settings_(settings), loader_(loader), model_(new ImageDirectoryModel(loader, this)) {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("ImageBrowserWindow"));
    setAcceptDrops(true);
    setWindowTitle(tr("图片浏览"));
    toolbar_ = addToolBar(tr("图片工具"));
    auto* toolbarStyle = new ToolbarStyle;
    toolbarStyle->setParent(toolbar_);
    toolbar_->setStyle(toolbarStyle);
    toolbar_->setMovable(false);
    toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    auto action = [this](const QString& title, const QString& icon, const QKeySequence& key,
                         std::function<void()> callback) {
        QIcon image = ThemeIcon::icon(icon);
        if (image.isNull()) image = style()->standardIcon(QStyle::SP_FileIcon);
        auto* a = toolbar_->addAction(image, title);
        a->setProperty("browserIcon", icon);
        a->setShortcut(key);
        a->setToolTip(key.isEmpty() ? title : title + QStringLiteral(" (%1)").arg(key.toString(QKeySequence::NativeText)));
        connect(a, &QAction::triggered, this, std::move(callback));
        return a;
    };
    navigationActions_ << action(tr("选择目录"), "browser-folder.png", QKeySequence("Ctrl+O"), [this] {
        const QString directory = QFileDialog::getExistingDirectory(this, tr("选择目录"), directory_);
        if (!directory.isEmpty() && discardOrSave()) openDirectory(directory);
    });
    navigationActions_ << action(tr("截图目录"), "browser-home.png", {}, [this] {
        if (discardOrSave()) screenshotDirectory();
    });
    navigationActions_ << action(tr("刷新"), "browser-refresh.png", QKeySequence("Ctrl+R"), [this] { refresh(); });
    auto* sidebar = action(tr("缩略图"), "browser-sidebar.png", {}, [] {});
    sidebar->setCheckable(true);
    sidebar->setChecked(true);
    connect(sidebar, &QAction::toggled, this, [this](bool checked) { list_->setVisible(checked); });
    toolbar_->addSeparator();
    navigationActions_ << action(tr("上一张"), "browser-previous.png", QKeySequence("Alt+Left"), [this] {
        selectRow(model_->indexOf(document_.path) - 1);
    });
    navigationActions_ << action(tr("下一张"), "browser-next.png", QKeySequence("Alt+Right"), [this] {
        selectRow(model_->indexOf(document_.path) + 1);
    });
    action(tr("适应窗口"), "browser-fit.png", QKeySequence("Ctrl+0"), [this] { canvas_->fit(); });
    auto* actual = action(tr("实际尺寸"), "browser-actual.png", QKeySequence("Ctrl+1"), [this] { canvas_->actualSize(); });
    if (auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(actual))) {
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setText(QStringLiteral("1:1"));
    }
    fullscreenAction_ = action(tr("全屏"), "browser-actual.png", QKeySequence("F11"), [this] { toggleFullscreen(); });
    fullscreenAction_->setCheckable(true);
    exitFullscreenAction_ = new QAction(this);
    exitFullscreenAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    exitFullscreenAction_->setEnabled(false);
    addAction(exitFullscreenAction_);
    connect(exitFullscreenAction_, &QAction::triggered, this, [this] {
        if (canvas_->cropping()) { canvas_->setCropping(false); updateActions(); }
        else if (isFullScreen()) toggleFullscreen();
    });
    toolbar_->addSeparator();
    cropAction_ = action(tr("裁剪"), "browser-crop.png", {}, [this] {
        canvas_->setCropping(cropAction_->isChecked()); updateActions();
    });
    cropAction_->setCheckable(true);
    editActions_ << cropAction_;
    applyCropAction_ = action(tr("应用裁剪"), "browser-apply.png", {}, [this] {
        if (!canvas_->cropRect().isEmpty()) edit({EditOperation::Crop, canvas_->cropRect(), {}});
    });
    cancelCropAction_ = action(tr("取消裁剪"), "browser-cancel.png", {}, [this] {
        canvas_->setCropping(false); updateActions();
    });
    editActions_ << action(tr("向左旋转"), "browser-rotate-left.png", {}, [this] { edit({EditOperation::RotateLeft, {}, {}}); });
    editActions_ << action(tr("向右旋转"), "browser-rotate-right.png", {}, [this] { edit({EditOperation::RotateRight, {}, {}}); });
    editActions_ << action(tr("水平翻转"), "browser-flip-horizontal.png", {}, [this] { edit({EditOperation::FlipHorizontal, {}, {}}); });
    editActions_ << action(tr("垂直翻转"), "browser-flip-vertical.png", {}, [this] { edit({EditOperation::FlipVertical, {}, {}}); });
    editActions_ << action(tr("调整图片尺寸"), "browser-resize.png", {}, [this] { resizeImage(); });
    toolbar_->addSeparator();
    undoAction_ = action(tr("撤销"), "browser-undo.png", QKeySequence::Undo, [this] {
        auto candidate = document_; --candidate.position; renderDocument(candidate);
    });
    redoAction_ = action(tr("重做"), "browser-redo.png", QKeySequence::Redo, [this] {
        auto candidate = document_; ++candidate.position; renderDocument(candidate);
    });
    resetAction_ = action(tr("恢复原图"), "browser-reset.png", {}, [this] {
        auto candidate = document_; candidate.position = 0; renderDocument(candidate);
    });
    toolbar_->addSeparator();
    playAction_ = action(tr("播放／暂停"), "browser-play.png", QKeySequence("Space"), [this] {
        if (!movie_) return;
        movie_->setPaused(movie_->state() == QMovie::Running);
        updateActions();
    });
    frameAction_ = action(tr("编辑当前帧"), "browser-frame.png", {}, [this] { freezeFrame(); });
    copyAction_ = action(tr("复制图片"), "browser-copy.png", QKeySequence::Copy, [this] {
        if (movie_) {
            const int frame = movie_->currentFrameNumber();
            const QString path = document_.path;
            busy_ = true;
            updateActions();
            loader_->run(this, [path, frame] { return ImageDocument::readFrame(path, frame); },
                         [this](const ImageResult& result) {
                busy_ = false;
                if (!result.error.isEmpty()) statusBar()->showMessage(result.error);
                else {
                    QApplication::clipboard()->setImage(result.image);
                    statusBar()->showMessage(tr("图片已复制"), 3000);
                }
                updateActions();
            });
        } else {
            QApplication::clipboard()->setImage(document_.current);
            statusBar()->showMessage(tr("图片已复制"), 3000);
        }
    });
    saveAction_ = action(tr("另存为"), "browser-save.png", QKeySequence::Save, [this] { save(); });
    convertAction_ = action(tr("格式转换"), "browser-convert.png", {}, [this] { convertFormat(); });
    auto* more = action(tr("更多"), "browser-more.png", {}, [] {});
    auto* menu = new QMenu(this);
    overwriteAction_ = menu->addAction(tr("覆盖原图"), this, [this] { save(true); });
    menu->addAction(tr("在资源管理器中定位"), this, [this] {
        if (!document_.path.isEmpty()) Util::locateFile(document_.path);
    });
    more->setMenu(menu);
    if (auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(more))) button->setPopupMode(QToolButton::InstantPopup);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    splitter_ = new QSplitter(central);
    list_ = new QListView(splitter_);
    list_->setModel(model_);
    list_->setItemDelegate(new ThumbnailDelegate(list_));
    list_->setUniformItemSizes(true);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->viewport()->installEventFilter(this);
    canvas_ = new ImageCanvas(splitter_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    layout->addWidget(splitter_, 1);
    setCentralWidget(central);
    details_ = new QLabel(this);
    details_->setObjectName(QStringLiteral("browserImageDetails"));
    details_->setTextFormat(Qt::PlainText);
    details_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    details_->setMinimumWidth(0);
    statusBar()->addWidget(details_, 1);
    details_->installEventFilter(this);
    for (QWidget* surface : {static_cast<QWidget*>(canvas_), list_->viewport(), static_cast<QWidget*>(toolbar_)}) {
        surface->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(surface, &QWidget::customContextMenuRequested, this, [this, surface](const QPoint& point) {
            if (surface == list_->viewport()) {
                const auto index = list_->indexAt(point);
                if (index.isValid()) {
                    selectRow(index.row());
                    if (document_.path != model_->pathAt(index.row())) return;
                }
            }
            showContextMenu(surface->mapToGlobal(point));
        });
    }
    refreshTimer_.setInterval(350);
    refreshTimer_.setSingleShot(true);
    thumbnailsTimer_.setInterval(30);
    thumbnailsTimer_.setSingleShot(true);
    connect(&directoryWatcher_, &QFileSystemWatcher::directoryChanged, this, [this] { refreshTimer_.start(); });
    connect(&directoryWatcher_, &QFileSystemWatcher::fileChanged, this, [this] { refreshTimer_.start(); });
    connect(&refreshTimer_, &QTimer::timeout, this, &ImageBrowserWindow::refresh);
    connect(&thumbnailsTimer_, &QTimer::timeout, this, &ImageBrowserWindow::requestThumbnails);
    connect(model_, &ImageDirectoryModel::scanned, this, &ImageBrowserWindow::listingReady);
    connect(list_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        if (!scanning_ && current.isValid()) selectRow(current.row());
    });
    connect(list_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { thumbnailsTimer_.start(); });
    connect(canvas_, &ImageCanvas::zoomChanged, this, &ImageBrowserWindow::updateStatus);
    connect(canvas_, &ImageCanvas::viewportChanged, this, &ImageBrowserWindow::updateStatus);
    connect(canvas_, &ImageCanvas::cropAccepted, this, [this](QRect rect) { edit({EditOperation::Crop, rect, {}}); });
    connect(canvas_, &ImageCanvas::cropCancelled, this, &ImageBrowserWindow::updateActions);
    connect(canvas_, &ImageCanvas::navigate, this, [this](int delta) { selectRow(model_->indexOf(document_.path) + delta); });
    applyDpi();
    screenshotDirectory();
    updateActions();
}

ImageBrowserWindow::~ImageBrowserWindow() { loadGeneration_->store(++imageVersion_); stopMovie(); }

void ImageBrowserWindow::applyDpi(double scale) {
    if (scale <= 0) scale = Util::getScreenScaleFactor(frameGeometry().center());
    toolbar_->setProperty("browserExtensionExtent", qRound(22 * scale) + 1);
    setStyleSheet(QStringLiteral("#ImageBrowserWindow, #ImageBrowserWindow QWidget { font-size: %1px; }").arg(qRound(13 * scale)));
    toolbar_->setStyleSheet(QStringLiteral(
        "QToolBar { spacing: 0; border: 0; padding: 0; } "
        "QToolBar QToolButton { min-width: 0; min-height: 0; padding: %1px; border: 0; border-radius: 0; background: transparent; } "
        "QToolBar QToolButton:hover, QToolBar QToolButton:pressed { border: 0; background: rgba(128,128,128,28); } "
        "QToolBar QToolButton:checked { border: 0; background: rgba(70,130,210,45); } "
        "QToolBar QToolButton::menu-indicator { image: none; width: 0; }")
        .arg(qRound(2 * scale)));
    toolbar_->setIconSize(QSize(qRound(20 * scale), qRound(20 * scale)));
    for (auto* a : toolbar_->actions()) {
        if (auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(a)))
            button->setFixedSize(qRound(28 * scale), qRound(30 * scale));
    }
    if (auto* extension = toolbar_->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"))) {
        extension->setFixedSize(qRound(22 * scale), qRound(30 * scale));
        extension->setStyleSheet(QStringLiteral("padding: 1px; min-width: 0; min-height: 0; border: 0;"));
        extension->setIcon(ThemeIcon::icon("browser-more.png"));
    }
    static_cast<ThumbnailDelegate*>(list_->itemDelegate())->scale = scale;
    QFont scaledFont = font();
    scaledFont.setPixelSize(qRound(13 * scale));
    setFont(scaledFont);
    list_->doItemsLayout();
    list_->setMinimumWidth(qRound(100 * scale));
    list_->setMaximumWidth(qRound(240 * scale));
    if (!isVisible()) {
        QScreen* screen = QGuiApplication::screenAt(frameGeometry().center());
        if (!screen) screen = QGuiApplication::primaryScreen();
        const QSize available = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
        resize(qMin(qRound(1100 * scale), available.width()), qMin(qRound(740 * scale), available.height()));
        splitter_->setSizes({qRound(144 * scale), qRound(956 * scale)});
    }
    setMinimumSize(qRound(540 * scale), qRound(380 * scale));
    QTimer::singleShot(0, this, [this, scale] {
        splitter_->setSizes({qRound(144 * scale), qMax(1, splitter_->width() - qRound(144 * scale))});
    });
    thumbnailsTimer_.start();
}

void ImageBrowserWindow::screenshotDirectory() {
    bool enabled = false;
    QString path;
    settings_->getAutoSaveImage(enabled, path);
    openDirectory(path);
}
void ImageBrowserWindow::openDirectory(const QString& directory, const QString& selection) {
    loadGeneration_->store(++imageVersion_);
    loading_ = false;
    stopMovie();
    document_ = {};
    displayedSize_ = {};
    animationSource_ = false;
    canvas_->setImage({}, true);
    directory_ = QDir(directory).absolutePath();
    pendingSelection_ = selection;
    const auto paths = directoryWatcher_.directories() + directoryWatcher_.files();
    if (!paths.isEmpty()) directoryWatcher_.removePaths(paths);
    if (QFileInfo(directory_).isDir()) directoryWatcher_.addPath(directory_);
    scanning_ = true;
    list_->setEnabled(false);
    canvas_->setMessage(tr("正在读取目录…"));
    model_->scan(directory_);
    updateActions();
}
void ImageBrowserWindow::refresh() {
    if (busy_ || scanning_) { refreshTimer_.start(); return; }
    pendingSelection_ = document_.path;
    scanning_ = true;
    list_->setEnabled(false);
    model_->scan(directory_);
}
void ImageBrowserWindow::listingReady(const QString& error) {
    scanning_ = false;
    list_->setEnabled(!busy_);
    if (!directoryWatcher_.directories().contains(directory_) && QFileInfo(directory_).isDir())
        directoryWatcher_.addPath(directory_);
    if (!document_.path.isEmpty() && QFileInfo::exists(document_.path) && !directoryWatcher_.files().contains(document_.path))
        directoryWatcher_.addPath(document_.path);
    if (!error.isEmpty() && document_.current.isNull()) canvas_->setMessage(error);
    else if (model_->rowCount() == 0 && document_.current.isNull()) canvas_->setMessage(tr("此目录中没有图片。"));
    int row = model_->indexOf(pendingSelection_);
    if (row < 0 && document_.current.isNull() && model_->rowCount()) row = 0;
    const QSignalBlocker blocker(list_->selectionModel());
    list_->setCurrentIndex(model_->index(row));
    if (row >= 0) {
        list_->scrollTo(model_->index(row));
        const QString path = model_->pathAt(row);
        if (document_.path != path || (!loading_ && !document_.dirty() && !document_.sourceUnchanged())) loadImage(path);
        else if (!document_.sourceUnchanged()) statusBar()->showMessage(tr("原文件已被外部修改，请将编辑结果另存为新文件。"));
    } else if (!document_.current.isNull()) statusBar()->showMessage(tr("原文件已不在当前目录中，仍可另存图片副本。"));
    thumbnailsTimer_.start();
    updateActions();
}
void ImageBrowserWindow::selectRow(int row) {
    if (busy_ || scanning_ || row < 0 || row >= model_->rowCount()) return;
    const QString path = model_->pathAt(row);
    if (path == document_.path) return;
    if (!discardOrSave()) {
        const QSignalBlocker blocker(list_->selectionModel());
        list_->setCurrentIndex(model_->index(model_->indexOf(document_.path)));
        return;
    }
    const int actualRow = model_->indexOf(path);
    {
        const QSignalBlocker blocker(list_->selectionModel());
        list_->setCurrentIndex(model_->index(actualRow));
    }
    list_->scrollTo(model_->index(actualRow));
    loadImage(path);
}
void ImageBrowserWindow::loadImage(const QString& path) {
    stopMovie();
    const quint64 version = ++imageVersion_;
    loadGeneration_->store(version);
    loading_ = true;
    animationSource_ = false;
    document_ = {};
    document_.path = path;
    displayedSize_ = {};
    canvas_->setImage({}, true);
    canvas_->setMessage(tr("正在加载图片…"));
    statusBar()->clearMessage();
    updateActions();
    const auto files = directoryWatcher_.files();
    if (!files.isEmpty()) directoryWatcher_.removePaths(files);
    if (QFileInfo::exists(path)) directoryWatcher_.addPath(path);
    const auto generation = loadGeneration_;
    loader_->run(this, [path, generation, version] {
        return generation->load() == version ? ImageDocument::read(path) : ImageResult{};
    }, [this, path, version](const ImageResult& result) {
        if (version != imageVersion_) return;
        loading_ = false;
        document_.load(path, result);
        displayedSize_ = result.originalSize;
        animationSource_ = result.animated;
        canvas_->setImage(result.image, true);
        if (!result.error.isEmpty()) canvas_->setMessage(tr("无法打开图片：%1").arg(result.error));
        if (result.limited) statusBar()->showMessage(tr("受限预览：超过 4000 万像素的图片不支持编辑或复制。"));
        if (result.animated && result.animationSupported && !result.limited) {
            QSize playbackSize = result.image.size();
            if (playbackSize.width() > 1600 || playbackSize.height() > 1600)
                playbackSize.scale(QSize(1600, 1600), Qt::KeepAspectRatio);
            playbackSize = playbackSize.expandedTo(QSize(1, 1));
            document_.original = document_.current = result.image.scaled(playbackSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            movie_ = new QMovie(path, QByteArray(), this);
            movie_->setCacheMode(QMovie::CacheNone);
            movie_->setScaledSize(playbackSize);
            connect(movie_, &QMovie::frameChanged, this, [this] {
                if (movie_) canvas_->setImage(movie_->currentImage());
            });
            connect(movie_, &QMovie::error, this, [this] {
                statusBar()->showMessage(tr("无法播放动图，当前显示静态帧。"));
                stopMovie(); updateActions();
            });
            if (isVisible()) movie_->start();
            else resumeMovie_ = true;
        } else if ((QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0 ||
                    QFileInfo(path).suffix().compare("webp", Qt::CaseInsensitive) == 0) && !result.animationSupported) {
            statusBar()->showMessage(tr("当前图片插件不支持动图播放。"));
        }
        updateActions();
    });
}
void ImageBrowserWindow::stopMovie() {
    resumeMovie_ = false;
    if (movie_) { movie_->stop(); movie_->deleteLater(); movie_ = nullptr; }
}
void ImageBrowserWindow::freezeFrame(std::function<void()> after) {
    if (!movie_ || movie_->currentImage().isNull()) return;
    const int frameNumber = movie_->currentFrameNumber();
    movie_->setPaused(true);
    const QString path = document_.path;
    busy_ = true;
    updateActions();
    loader_->run(this, [path, frameNumber] { return ImageDocument::readFrame(path, frameNumber); },
                 [this, after](const ImageResult& result) {
        busy_ = false;
        if (!result.error.isEmpty()) statusBar()->showMessage(result.error);
        else {
            stopMovie();
            document_.original = document_.current = result.image;
            document_.operations.clear();
            document_.position = 0;
            document_.frozenFrame = true;
            displayedSize_ = result.image.size();
            canvas_->setImage(result.image, true);
            statusBar()->showMessage(tr("正在编辑静态帧，请另存为 PNG 或 JPEG 图片。"));
        }
        updateActions();
        if (result.error.isEmpty() && after) after();
    });
}
void ImageBrowserWindow::requestThumbnails() {
    if (scanning_ || !list_->isVisible()) return;
    int first = list_->indexAt(QPoint(4, 4)).row();
    if (first < 0) first = 0;
    const double scale = static_cast<ThumbnailDelegate*>(list_->itemDelegate())->scale;
    const int perPage = qMax(1, list_->viewport()->height() / qMax(1, qRound(112 * scale)) + 1);
    model_->requestRange(qMax(0, first - perPage), first + 2 * perPage,
                         QSize(qRound(128 * scale), qRound(80 * scale)));
}
void ImageBrowserWindow::updateStatus() {
    const int row = model_->indexOf(document_.path);
    const QString name = QFileInfo(document_.path).fileName();
    const QString fileSize = document_.fileSize >= 0 ? QLocale().formattedDataSize(document_.fileSize) : QStringLiteral("-");
    const QString text = name.isEmpty() ? QString() : QStringLiteral("%1 x %2   %3   %4 / %5   %6%")
        .arg(qMax(0, displayedSize_.width())).arg(qMax(0, displayedSize_.height())).arg(fileSize)
        .arg(row + 1).arg(model_->rowCount()).arg(qRound(canvas_->zoom() * 100));
    details_->setText(details_->fontMetrics().elidedText(text, Qt::ElideMiddle, qMax(0, details_->contentsRect().width())));
    details_->setToolTip(text);
    setWindowTitle(name.isEmpty() ? tr("图片浏览") : name + (document_.dirty() ? " * - " : " - ") + tr("图片浏览"));
}
void ImageBrowserWindow::toggleFullscreen() {
    if (isFullScreen()) {
        if (restoreMaximized_) showMaximized();
        else showNormal();
    } else {
        restoreMaximized_ = isMaximized();
        showFullScreen();
    }
    fullscreenAction_->setChecked(isFullScreen());
    exitFullscreenAction_->setEnabled(isFullScreen());
}
void ImageBrowserWindow::updateActions() {
    if (auto* extension = toolbar_->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button")))
        extension->setIcon(ThemeIcon::icon("browser-more.png"));
    for (auto* a : toolbar_->actions()) {
        const QString icon = a->property("browserIcon").toString();
        if (!icon.isEmpty()) a->setIcon(ThemeIcon::icon(icon));
        if (icon == QStringLiteral("browser-actual.png")) {
            if (auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(a)); button && button->toolButtonStyle() == Qt::ToolButtonTextOnly)
                button->setText(QStringLiteral("1:1"));
        }
    }
    const bool available = !document_.current.isNull() && !loading_ && !busy_;
    const bool editable = available && !document_.limited && !movie_;
    for (auto* action : editActions_) action->setEnabled(editable);
    for (auto* action : navigationActions_) action->setEnabled(!busy_);
    saveAction_->setEnabled(available && !document_.limited && !movie_);
    copyAction_->setEnabled(available && !document_.limited);
    convertAction_->setEnabled(available && !document_.limited);
    overwriteAction_->setEnabled(editable && !animationSource_ && document_.sourceUnchanged() &&
        (QFileInfo(document_.path).suffix().toLower() == "png" || QFileInfo(document_.path).suffix().toLower() == "jpg" || QFileInfo(document_.path).suffix().toLower() == "jpeg"));
    undoAction_->setEnabled(editable && document_.canUndo());
    redoAction_->setEnabled(editable && document_.canRedo());
    resetAction_->setEnabled(editable && document_.canUndo());
    playAction_->setVisible(movie_ != nullptr);
    playAction_->setEnabled(movie_ && !busy_);
    playAction_->setIcon(ThemeIcon::icon(movie_ && movie_->state() == QMovie::Running ? "browser-pause.png" : "browser-play.png"));
    frameAction_->setVisible(movie_ != nullptr);
    frameAction_->setEnabled(movie_ && available);
    cropAction_->setChecked(canvas_->cropping());
    applyCropAction_->setVisible(canvas_->cropping());
    cancelCropAction_->setVisible(canvas_->cropping());
    applyCropAction_->setEnabled(editable);
    cancelCropAction_->setEnabled(!busy_);
    list_->setEnabled(!busy_ && !scanning_);
    updateStatus();
}
bool ImageBrowserWindow::discardOrSave() {
    if (!document_.dirty()) return true;
    QMessageBox dialog(QMessageBox::Question, tr("未保存的编辑"), tr("离开当前图片前，要保存编辑结果吗？"), QMessageBox::NoButton, this);
    auto* saveButton = dialog.addButton(tr("另存为"), QMessageBox::AcceptRole);
    auto* discardButton = dialog.addButton(tr("放弃"), QMessageBox::DestructiveRole);
    dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(saveButton);
    dialog.exec();
    if (dialog.clickedButton() == saveButton) return save(false, false);
    return dialog.clickedButton() == discardButton;
}
bool ImageBrowserWindow::save(bool overwrite, bool refreshListing, const QByteArray& requestedFormat) {
    if (busy_ || loading_ || document_.current.isNull() || document_.limited || movie_) return false;
    QString path = document_.path;
    QByteArray format = requestedFormat.isEmpty() ? QByteArray("png") : requestedFormat;
    if (overwrite) {
        if (animationSource_ || !document_.sourceUnchanged()) {
            QMessageBox::warning(this, tr("无法覆盖"), tr("原文件已被外部修改，请将编辑结果另存为新文件。"));
            return false;
        }
        if (QMessageBox::question(this, tr("覆盖原图"), tr("确定替换原始图片吗？"), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return false;
        format = QFileInfo(path).suffix().toLower().toLatin1();
    } else {
        QFileDialog dialog(this, requestedFormat.isEmpty() ? tr("另存为") : tr("格式转换"), ImageDocument::suggestedPath(document_.path, format));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        if (requestedFormat.isEmpty()) dialog.setNameFilters({tr("PNG 图片 (*.png)"), tr("JPEG 图片 (*.jpg *.jpeg)")});
        else if (format == "jpeg") dialog.setNameFilter(tr("JPEG 图片 (*.jpg *.jpeg)"));
        else dialog.setNameFilter(QStringLiteral("%1 (*.%2)").arg(QString::fromLatin1(format).toUpper(), QString::fromLatin1(format)));
        dialog.setDefaultSuffix(format == "jpeg" ? "jpg" : QString::fromLatin1(format));
        connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog, requestedFormat](const QString& filter) {
            dialog.setDefaultSuffix(filter.contains("JPEG") ? "jpg" : requestedFormat.isEmpty() ? "png" : QString::fromLatin1(requestedFormat));
        });
        if (!dialog.exec() || dialog.selectedFiles().isEmpty()) return false;
        path = dialog.selectedFiles().first();
        const QString extension = QFileInfo(path).suffix().toLower();
        const QByteArray selectedFormat = extension == "jpg" ? QByteArray("jpeg") : extension.toLatin1();
        if ((requestedFormat.isEmpty() && selectedFormat != "png" && selectedFormat != "jpeg") ||
            (!requestedFormat.isEmpty() && selectedFormat != requestedFormat)) {
            QMessageBox::warning(this, tr("另存为"), requestedFormat.isEmpty() ? tr("请选择 PNG 或 JPEG 文件名。") : tr("文件扩展名必须与所选格式一致。"));
            return false;
        }
        format = selectedFormat;
        if (QFileInfo(path).absoluteFilePath().compare(QFileInfo(document_.path).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, tr("另存为"), tr("请选择新文件名，或使用“覆盖原图”。"));
            return false;
        }
    }
    if ((format == "jpeg" || format == "jpg" || format == "bmp") && document_.current.hasAlphaChannel()) {
        if (QMessageBox::question(this, tr("透明区域"), tr("此格式会将透明区域替换为白色，是否继续？"), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return false;
    }
    if (overwrite && !document_.sourceUnchanged()) return false;
    busy_ = true;
    updateActions();
    SaveProgress progress(this);
    auto result = std::make_shared<SaveResult>();
    const QImage image = document_.current;
    const QDateTime expectedModified = overwrite ? document_.modified : QDateTime();
    const qint64 expectedSize = document_.fileSize;
    loader_->run(&progress, [path, image, format, expectedModified, expectedSize] {
        return ImageDocument::save(path, image, format, expectedModified, expectedSize);
    },
                 [&progress, result](const SaveResult& saved) { *result = saved; progress.accept(); });
    progress.exec();
    busy_ = false;
    if (!result->success) {
        QMessageBox::warning(this, tr("保存失败"), result->error);
        updateActions();
        return false;
    }
    ImageResult saved;
    saved.image = image;
    saved.modified = QFileInfo(path).lastModified();
    saved.fileSize = QFileInfo(path).size();
    document_.load(path, saved);
    animationSource_ = false;
    displayedSize_ = image.size();
    statusBar()->showMessage(tr("图片已保存"), 3000);
    if (refreshListing) {
        if (QFileInfo(path).absolutePath() != directory_) openDirectory(QFileInfo(path).absolutePath(), path);
        else { loadImage(path); refresh(); }
    }
    updateActions();
    return true;
}
void ImageBrowserWindow::convertFormat() {
    if (busy_ || loading_ || document_.current.isNull() || document_.limited) return;
    if (movie_) {
        if (QMessageBox::question(this, tr("格式转换"), tr("将当前帧转换为静态图片，不保留动画，是否继续？"),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        freezeFrame([this] { convertFormat(); });
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("格式转换"));
    auto* form = new QFormLayout(&dialog);
    auto* formats = new QComboBox(&dialog);
    const auto supported = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : {QByteArray("png"), QByteArray("jpeg"), QByteArray("bmp"), QByteArray("webp")}) {
        if (supported.contains(format)) formats->addItem(QString::fromLatin1(format).toUpper(), format);
    }
    form->addRow(tr("目标格式"), formats);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() && formats->currentIndex() >= 0) save(false, true, formats->currentData().toByteArray());
}
void ImageBrowserWindow::showContextMenu(const QPoint& globalPosition) {
    updateActions();
    QMenu menu(this);
    for (auto* action : toolbar_->actions()) {
        if (action->isSeparator()) menu.addSeparator();
        else if (action->menu()) {
            menu.addSeparator();
            menu.addActions(action->menu()->actions());
        } else if (action->isVisible()) menu.addAction(action);
    }
    menu.exec(globalPosition);
}
void ImageBrowserWindow::contextMenuEvent(QContextMenuEvent* event) {
    showContextMenu(event->globalPos());
    event->accept();
}
void ImageBrowserWindow::edit(const EditOperation& operation) {
    if (busy_ || loading_ || document_.limited || movie_ || document_.current.isNull()) return;
    auto candidate = document_;
    candidate.append(operation);
    renderDocument(candidate);
}
void ImageBrowserWindow::renderDocument(ImageDocument candidate) {
    if (busy_) return;
    busy_ = true;
    canvas_->setCropping(false);
    const quint64 version = ++imageVersion_;
    updateActions();
    statusBar()->showMessage(tr("正在处理图片…"));
    loader_->run(this, [base = candidate.original, operations = candidate.operations, count = candidate.position] {
        return ImageDocument::render(base, operations, count);
    }, [this, candidate, version](const ImageResult& result) mutable {
        if (version != imageVersion_) return;
        busy_ = false;
        if (!result.error.isEmpty()) statusBar()->showMessage(result.error);
        else {
            candidate.current = result.image;
            document_ = candidate;
            displayedSize_ = result.image.size();
            canvas_->setImage(result.image, true);
            statusBar()->clearMessage();
        }
        updateActions();
    });
}
void ImageBrowserWindow::resizeImage() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("调整图片尺寸"));
    auto* form = new QFormLayout(&dialog);
    auto* width = new QSpinBox(&dialog);
    auto* height = new QSpinBox(&dialog);
    width->setRange(1, 40000000);
    height->setRange(1, 40000000);
    width->setValue(document_.current.width());
    height->setValue(document_.current.height());
    auto* lock = new QCheckBox(tr("锁定宽高比"), &dialog);
    lock->setChecked(true);
    const double ratio = double(document_.current.width()) / document_.current.height();
    connect(width, &QSpinBox::valueChanged, &dialog, [height, lock, ratio](int value) {
        if (lock->isChecked()) { const QSignalBlocker blocker(height); height->setValue(qRound(qBound(1.0, value / ratio, 40000000.0))); }
    });
    connect(height, &QSpinBox::valueChanged, &dialog, [width, lock, ratio](int value) {
        if (lock->isChecked()) { const QSignalBlocker blocker(width); width->setValue(qRound(qBound(1.0, value * ratio, 40000000.0))); }
    });
    form->addRow(tr("宽度（像素）"), width);
    form->addRow(tr("高度（像素）"), height);
    form->addRow(lock);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    auto validate = [width, height, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(qint64(width->value()) * height->value() <= MaxEditablePixels);
    };
    connect(width, &QSpinBox::valueChanged, &dialog, validate);
    connect(height, &QSpinBox::valueChanged, &dialog, validate);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() && QSize(width->value(), height->value()) != document_.current.size())
        edit({EditOperation::Resize, {}, QSize(width->value(), height->value())});
}
void ImageBrowserWindow::closeEvent(QCloseEvent* event) {
    if (busy_ || !discardOrSave()) { event->ignore(); return; }
    loadGeneration_->store(++imageVersion_);
    stopMovie();
    event->accept();
}
void ImageBrowserWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!busy_ && event->mimeData()->hasUrls()) event->acceptProposedAction();
}
void ImageBrowserWindow::dropEvent(QDropEvent* event) {
    if (busy_) return;
    const auto urls = event->mimeData()->urls();
    QString directory, selection;
    for (const auto& url : urls) {
        if (!url.isLocalFile()) continue;
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()) { directory = info.absoluteFilePath(); break; }
        QImageReader reader(info.absoluteFilePath());
        if (info.isFile() && reader.canRead()) {
            directory = info.absolutePath(); selection = info.absoluteFilePath(); break;
        }
    }
    if (directory.isEmpty()) { statusBar()->showMessage(tr("请拖入本地图片或目录。"), 4000); return; }
    if (!discardOrSave()) return;
    openDirectory(directory, selection);
    if (urls.size() > 1) statusBar()->showMessage(tr("已打开第一个有效项目，忽略了 %1 项。").arg(urls.size() - 1), 6000);
    event->acceptProposedAction();
}
void ImageBrowserWindow::hideEvent(QHideEvent* event) {
    if (movie_ && movie_->state() == QMovie::Running) { resumeMovie_ = true; movie_->setPaused(true); }
    QMainWindow::hideEvent(event);
}
void ImageBrowserWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (windowHandle() && !windowHandle()->property("imageBrowserDpiConnected").toBool()) {
        windowHandle()->setProperty("imageBrowserDpiConnected", true);
        connect(windowHandle(), &QWindow::screenChanged, this, [this] { applyDpi(); });
        applyDpi();
    }
    if (movie_ && resumeMovie_) { resumeMovie_ = false; movie_->start(); }
    thumbnailsTimer_.start();
}
void ImageBrowserWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && fullscreenAction_) {
        fullscreenAction_->setChecked(isFullScreen());
        exitFullscreenAction_->setEnabled(isFullScreen());
    }
    if (event->type() == QEvent::WindowStateChange && movie_) {
        if (isMinimized() && movie_->state() == QMovie::Running) { resumeMovie_ = true; movie_->setPaused(true); }
        else if (!isMinimized() && resumeMovie_) { resumeMovie_ = false; movie_->setPaused(false); }
    }
    if (event->type() == QEvent::ScreenChangeInternal) applyDpi();
    if (event->type() == QEvent::PaletteChange && details_) { updateActions(); update(); list_->viewport()->update(); }
}
bool ImageBrowserWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == details_ && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &ImageBrowserWindow::updateStatus);
    if (watched == list_->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        thumbnailsTimer_.start(); updateStatus();
    }
    return QMainWindow::eventFilter(watched, event);
}
} // namespace ImageBrowser
