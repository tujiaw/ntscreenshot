#include "ImageBrowserWindow.h"
#include <QPointer>
#include "ImageCanvas.h"
#include "ImageEditorPanel.h"
#include "ImageConversionDialog.h"
#include "ImageDirectoryModel.h"
#include "EditorValueSpinBox.h"
#include "ImageLoader.h"
#include "core/platform/Util.h"
#include "core/platform/SystemBackground.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"
#include "core/theme/UiStyler.h"

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
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
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
#include <utility>

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
    toolbar_->setObjectName(QStringLiteral("ImageBrowserToolBar"));
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
        connect(a, &QAction::triggered, this, [this, callback = std::move(callback)] { dispatchAction(callback); });
        return a;
    };
    // 顶栏按钮可以是「图标 + 文字」，文字优先取 browserText（QAction 的 text 要留给测试和快捷键）。
    auto barButton = [this](QAction* a, const QString& text, bool primary = false) {
        auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(a));
        if (!button) return;
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        if (!text.isEmpty()) {
            button->setProperty("browserText", text);
            button->setText(text);
        }
        if (primary) UiStyler::setRole(button, UiRole::PrimaryButton);
    };
    editorAction_ = action(tr("编辑"), {}, {}, [this] {
        editorPanel_->setVisible(editorAction_->isChecked());
        if (!editorAction_->isChecked() && regionFromPanel_) canvas_->setCropping(false);
        positionEditorPanel();
        updateActions();
    });
    editorAction_->setIcon(ImageEditorPanel::editIcon());
    editorAction_->setCheckable(true);
    barButton(editorAction_, tr("编辑"), true);
    cropAction_ = new QAction(ThemeIcon::icon("browser-crop.png"), tr("裁剪"), this);
    cropAction_->setProperty("browserIcon", "browser-crop.png");
    addAction(cropAction_);
    connect(cropAction_, &QAction::triggered, this, [this] { dispatchAction([this] {
        regionOperation_ = {};
        regionFromPanel_ = false;
        canvas_->setCropping(cropAction_->isChecked());
        if (canvas_->cropping()) showStatus(tr("拖动选择区域，按 Enter 应用，Esc 取消。"));
        updateActions();
    }); });
    cropAction_->setCheckable(true);
    editActions_ << cropAction_;
    toolbar_->addSeparator();
    navigationActions_ << action(tr("选择目录"), "browser-folder.png", QKeySequence("Ctrl+O"), [this] {
        const QString directory = QFileDialog::getExistingDirectory(this, tr("选择目录"), directory_);
        if (!directory.isEmpty() && discardOrSave()) openDirectory(directory);
    });
    // 「截图目录」不占顶栏位置，但功能保留：走窗口级 action，右键菜单里仍能进入。
    screenshotDirectoryAction_ = new QAction(ThemeIcon::icon("browser-home.png"), tr("截图目录"), this);
    if (screenshotDirectoryAction_->icon().isNull())
        screenshotDirectoryAction_->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    screenshotDirectoryAction_->setProperty("browserIcon", "browser-home.png");
    screenshotDirectoryAction_->setToolTip(tr("截图目录"));
    connect(screenshotDirectoryAction_, &QAction::triggered, this, [this] {
        dispatchAction([this] { if (discardOrSave()) screenshotDirectory(); });
    });
    addAction(screenshotDirectoryAction_);
    navigationActions_ << screenshotDirectoryAction_;
    navigationActions_ << action(tr("刷新"), "browser-refresh.png", QKeySequence("Ctrl+R"), [this] { refresh(); });
    for (auto* a : std::as_const(navigationActions_)) barButton(a, a->text());
    // 弹性空档把右侧操作推到工具栏远端，它的几何区域同时就是居中文件名可用的横向区间。
    spacerAction_ = toolbar_->addWidget(new QWidget(toolbar_));
    spacerAction_->setProperty("browserSpacer", true);
    if (auto* spacer = toolbar_->widgetForAction(spacerAction_))
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addSeparator();
    convertAction_ = action(tr("格式转换"), "browser-convert.png", {}, [this] { convertFormat(); });
    barButton(convertAction_, convertAction_->text());
    auto* more = action(tr("更多"), "browser-more.png", {}, [] {});
    auto* menu = new QMenu(this);
    wallpaperAction_ = menu->addAction(tr("设为桌面背景"), this, [this] { dispatchAction([this] { setSystemBackground(false); }); });
    lockScreenAction_ = menu->addAction(tr("设为锁屏背景"), this, [this] { dispatchAction([this] { setSystemBackground(true); }); });
    menu->addSeparator();
    overwriteAction_ = menu->addAction(tr("覆盖原图"), this, [this] { dispatchAction([this] { save(true); }); });
    menu->addAction(tr("在资源管理器中定位"), this, [this] {
        if (!document_.path.isEmpty()) Util::locateFile(document_.path);
    });
    more->setMenu(menu);
    if (auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(more))) button->setPopupMode(QToolButton::InstantPopup);
    titleLabel_ = new QLabel(toolbar_);
    titleLabel_->setObjectName(QStringLiteral("browserTitleLabel"));
    titleLabel_->setTextFormat(Qt::PlainText);
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    toolbar_->installEventFilter(this);
    exitFullscreenAction_ = new QAction(this);
    exitFullscreenAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    exitFullscreenAction_->setEnabled(false);
    addAction(exitFullscreenAction_);
    connect(exitFullscreenAction_, &QAction::triggered, this, [this] {
        if (canvas_->cancelDrawing()) return;
        if (canvas_->cropping()) { canvas_->setCropping(false); updateActions(); }
        else if (editorPanel_->isVisible()) { editorPanel_->hide(); editorAction_->setChecked(false); positionEditorPanel(); updateActions(); }
        else if (isFullScreen()) toggleFullscreen();
    });

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
    canvas_->installEventFilter(this);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    layout->addWidget(splitter_, 1);
    setCentralWidget(central);

    // 翻页箭头挂在画布上，默认隐藏，鼠标进入画布才浮出来，不常驻遮挡图片。
    overlayPrevious_ = new QToolButton(canvas_);
    overlayNext_ = new QToolButton(canvas_);
    for (QToolButton* button : {overlayPrevious_, overlayNext_}) {
        button->setObjectName(QStringLiteral("browserOverlayButton"));
        // 箭头铺在半透明黑色底衬上，底衬深/浅主题都不变，所以固定用白色图标而不是主题色。
        button->setIcon(ThemeIcon::icon(button == overlayPrevious_ ? "browser-previous.png" : "browser-next.png",
                                        IconTone::OnAccent));
        button->setToolTip(button == overlayPrevious_ ? tr("上一张") : tr("下一张"));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::ArrowCursor);
        button->installEventFilter(this);
        button->hide();
    }
    connect(overlayPrevious_, &QToolButton::clicked, this, [this] { selectRow(model_->indexOf(document_.path) - 1); });
    connect(overlayNext_, &QToolButton::clicked, this, [this] { selectRow(model_->indexOf(document_.path) + 1); });

    // 底部翻页靠左、编辑工具居中，缩放、保存和图片信息靠右。
    auto panelAction = [this](const QString& title, const QString& icon, const QKeySequence& key,
                              std::function<void()> callback) {
        QIcon image = ThemeIcon::icon(icon);
        if (image.isNull()) image = style()->standardIcon(QStyle::SP_FileIcon);
        auto* a = new QAction(image, title, this);
        a->setProperty("browserIcon", icon);
        a->setShortcut(key);
        a->setToolTip(key.isEmpty() ? title : title + QStringLiteral(" (%1)").arg(key.toString(QKeySequence::NativeText)));
        connect(a, &QAction::triggered, this, [this, callback = std::move(callback)] { dispatchAction(callback); });
        addAction(a);
        return a;
    };

    statusBar()->setSizeGripEnabled(false);
    controlBar_ = new QWidget(this);
    controlBar_->installEventFilter(this);
    auto* controlRow = new QHBoxLayout(controlBar_);
    controlRow->setContentsMargins(0, 0, 0, 0);
    controlRow->setSpacing(0);
    controlLeft_ = new QWidget(controlBar_);
    controlLeft_->setObjectName(QStringLiteral("browserControlLeft"));
    controlRight_ = new QWidget(controlBar_);
    controlRight_->setObjectName(QStringLiteral("browserControlRight"));
    auto* leftRow = new QHBoxLayout(controlLeft_);
    auto* rightRow = new QHBoxLayout(controlRight_);
    for (QHBoxLayout* row : {leftRow, rightRow}) { row->setContentsMargins(0, 0, 0, 0); row->setSpacing(0); }

    sidebarAction_ = panelAction(tr("缩略图"), "browser-sidebar.png", {}, [this] {
        if (list_) list_->setVisible(sidebarAction_->isChecked());
    });
    sidebarAction_->setCheckable(true);
    sidebarAction_->setChecked(true);
    sidebarButton_ = controlButton(sidebarAction_);
    previousAction_ = panelAction(tr("上一张"), "browser-previous.png", QKeySequence("Alt+Left"), [this] {
        selectRow(model_->indexOf(document_.path) - 1);
    });
    nextAction_ = panelAction(tr("下一张"), "browser-next.png", QKeySequence("Alt+Right"), [this] {
        selectRow(model_->indexOf(document_.path) + 1);
    });
    counterLabel_ = new QLabel(controlLeft_);
    counterLabel_->setObjectName(QStringLiteral("browserImageCounter"));
    counterLabel_->setTextFormat(Qt::PlainText);
    counterLabel_->setAlignment(Qt::AlignCenter);
    controlCopySave_ = new QWidget(controlRight_);
    auto* copySaveRow = new QHBoxLayout(controlCopySave_);
    copySaveRow->setContentsMargins(0, 0, 0, 0);
    copySaveRow->setSpacing(0);
    copyAction_ = panelAction(tr("复制图片"), "browser-copy.png", QKeySequence::Copy, [this] {
        if (movie_) {
            const int frame = movie_->currentFrameNumber();
            const QString path = document_.path;
            setBusy(true);
            loader_->run(this, [path, frame] { return ImageDocument::readFrame(path, frame); },
                         [this](const ImageResult& result) {
                setBusy(false);
                if (!result.error.isEmpty()) showStatus(result.error);
                else {
                    QApplication::clipboard()->setImage(result.image);
                    showStatus(tr("图片已复制"), 3000);
                }
                updateActions();
            });
        } else {
            QApplication::clipboard()->setImage(document_.current);
            showStatus(tr("图片已复制"), 3000);
        }
    });
    saveAction_ = panelAction(tr("另存为"), "browser-save.png", QKeySequence::Save, [this] { save(); });
    // 只出现在右键菜单底部，所以不进工具栏，也不挂快捷键。
    deleteAction_ = new QAction(ThemeIcon::icon(QStringLiteral("remove.png"), IconTone::Danger), tr("删除图片"), this);
    deleteAction_->setObjectName(QStringLiteral("browserDelete"));
    deleteAction_->setProperty("browserIcon", QStringLiteral("remove.png"));
    connect(deleteAction_, &QAction::triggered, this, &ImageBrowserWindow::deleteImage);
    details_ = new QLabel(controlLeft_);
    details_->setObjectName(QStringLiteral("browserImageDetails"));
    details_->setTextFormat(Qt::PlainText);
    // Preferred 而不是 Ignored：Ignored 会让布局把宽度压到 0，文件信息只剩 "1600... KiB"。
    // 空间不够时由 layoutControlBar 整条隐藏，不靠这个标签自己缩。
    details_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    details_->installEventFilter(this);
    statusLabel_ = new QLabel(controlLeft_);
    statusLabel_->setObjectName(QStringLiteral("browserStatusMessage"));
    statusLabel_->setTextFormat(Qt::PlainText);
    statusLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    statusLabel_->installEventFilter(this);

    leftRow->addWidget(sidebarButton_);
    auto* saveSeparator = new QFrame(controlCopySave_);
    saveSeparator->setObjectName(QStringLiteral("browserControlSeparator"));
    saveSeparator->setFrameShape(QFrame::VLine);
    copySaveRow->addWidget(saveSeparator);
    copySaveRow->addWidget(controlButton(copyAction_));
    copySaveRow->addWidget(controlButton(saveAction_));

    toolPill_ = new QFrame(controlBar_);
    toolPill_->setObjectName(QStringLiteral("browserToolPill"));
    auto* pillRow = new QHBoxLayout(toolPill_);
    pillRow->setContentsMargins(0, 0, 0, 0);
    pillRow->setSpacing(0);
    auto pillSeparator = [this] {
        auto* line = new QFrame(toolPill_);
        line->setObjectName(QStringLiteral("browserToolPillSeparator"));
        line->setFrameShape(QFrame::VLine);
        return line;
    };
    applyCropAction_ = panelAction(tr("应用裁剪"), "browser-apply.png", {}, [this] {
        applyEditRegion(canvas_->cropRect());
    });
    cancelCropAction_ = panelAction(tr("取消裁剪"), "browser-cancel.png", {}, [this] {
        canvas_->setCropping(false); showStatus(QString()); updateActions();
    });
    editActions_ << panelAction(tr("向左旋转"), "browser-rotate-left.png", {}, [this] { edit({EditOperation::RotateLeft, {}, {}}); });
    editActions_ << panelAction(tr("向右旋转"), "browser-rotate-right.png", {}, [this] { edit({EditOperation::RotateRight, {}, {}}); });
    editActions_ << panelAction(tr("水平翻转"), "browser-flip-horizontal.png", {}, [this] { edit({EditOperation::FlipHorizontal, {}, {}}); });
    editActions_ << panelAction(tr("垂直翻转"), "browser-flip-vertical.png", {}, [this] { edit({EditOperation::FlipVertical, {}, {}}); });
    editActions_ << panelAction(tr("调整图片尺寸"), "browser-resize.png", {}, [this] { resizeImage(); });
    undoAction_ = panelAction(tr("撤销"), "browser-undo.png", QKeySequence::Undo, [this] {
        auto candidate = document_; --candidate.position; renderDocument(candidate);
    });
    redoAction_ = panelAction(tr("重做"), "browser-redo.png", QKeySequence::Redo, [this] {
        auto candidate = document_; ++candidate.position; renderDocument(candidate);
    });
    resetAction_ = panelAction(tr("恢复原图"), "browser-reset.png", {}, [this] {
        auto candidate = document_; candidate.position = 0; renderDocument(candidate);
    });
    pillRow->addWidget(controlButton(applyCropAction_));
    pillRow->addWidget(controlButton(cancelCropAction_));
    for (QAction* action : std::as_const(editActions_)) {
        if (action == editActions_.first()) pillRow->addWidget(pillSeparator());
        pillRow->addWidget(controlButton(action));
    }
    pillRow->addWidget(pillSeparator());
    for (QAction* action : {undoAction_, redoAction_, resetAction_}) pillRow->addWidget(controlButton(action));

    playAction_ = panelAction(tr("播放／暂停"), "browser-play.png", QKeySequence("Space"), [this] {
        if (!movie_) return;
        movie_->setPaused(movie_->state() == QMovie::Running);
        updateActions();
    });
    frameAction_ = panelAction(tr("编辑当前帧"), "browser-frame.png", {}, [this] { freezeFrame(); });
    // 缩放档位必须是 QToolButton + QMenu：QComboBox 内部含 QListView，会抢在缩略图列表前面被测试找到。
    zoomButton_ = new QToolButton(controlRight_);
    zoomButton_->setObjectName(QStringLiteral("browserZoomButton"));
    zoomButton_->setAutoRaise(true);
    zoomButton_->setFocusPolicy(Qt::NoFocus);
    zoomButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    zoomButton_->setPopupMode(QToolButton::InstantPopup);
    zoomButton_->setToolTip(tr("缩放"));
    auto* zoomMenu = new QMenu(zoomButton_);
    fitAction_ = panelAction(tr("适应窗口"), "browser-fit.png", QKeySequence("Ctrl+0"), [this] { canvas_->fit(); });
    actualAction_ = panelAction(tr("实际尺寸"), "browser-actual.png", QKeySequence("Ctrl+1"), [this] { canvas_->actualSize(); });
    zoomMenu->addAction(fitAction_);
    zoomMenu->addSeparator();
    for (int percent : {50, 100, 200, 400}) {
        QAction* preset = zoomMenu->addAction(QStringLiteral("%1%").arg(percent));
        connect(preset, &QAction::triggered, this, [this, percent] { canvas_->setZoom(percent / 100.0); });
    }
    zoomMenu->addSeparator();
    zoomMenu->addAction(actualAction_);
    zoomButton_->setMenu(zoomMenu);
    zoomOutAction_ = panelAction(tr("缩小"), "browser-zoom-out.png", QKeySequence::ZoomOut, [this] { setZoomStep(1 / 1.15); });
    zoomInAction_ = panelAction(tr("放大"), "browser-zoom-in.png", QKeySequence::ZoomIn, [this] { setZoomStep(1.15); });
    fullscreenAction_ = panelAction(tr("全屏"), "browser-fit.png", QKeySequence("F11"), [this] { toggleFullscreen(); });
    fullscreenAction_->setCheckable(true);

    leftRow->addWidget(controlButton(previousAction_));
    leftRow->addWidget(counterLabel_);
    leftRow->addWidget(controlButton(nextAction_));
    leftRow->addWidget(controlButton(playAction_));
    leftRow->addWidget(controlButton(frameAction_));
    leftRow->addWidget(statusLabel_);
    leftRow->addStretch(1);
    rightRow->addStretch(1);
    rightRow->addWidget(controlButton(zoomOutAction_));
    rightRow->addWidget(zoomButton_);
    rightRow->addWidget(controlButton(zoomInAction_));
    rightRow->addWidget(controlButton(fullscreenAction_));
    rightRow->addWidget(controlCopySave_);
    rightRow->addWidget(details_);
    details_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    controlRow->addWidget(controlLeft_, 1);
    controlRow->addWidget(toolPill_);
    controlRow->addWidget(controlRight_, 1);
    statusBar()->addWidget(controlBar_, 1);
    statusTimer_.setSingleShot(true);
    connect(&statusTimer_, &QTimer::timeout, this, [this] { statusText_.clear(); layoutControlBar(); updateStatus(); });
    // 只有超过 1 秒的操作才提示：快操作闪一下等待光标和面板文案反而更烦。
    busyCursorTimer_.setSingleShot(true);
    connect(&busyCursorTimer_, &QTimer::timeout, this, [this] {
        if (!busy_ || busyHintShown_) return;
        busyHintShown_ = true;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        if (editorPanel_) editorPanel_->setBusy(true);
        if (!busyMessage_.isEmpty()) showStatus(busyMessage_);
    });
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
    connect(&refreshTimer_, &QTimer::timeout, this, [this] { refresh(); });
    connect(&thumbnailsTimer_, &QTimer::timeout, this, &ImageBrowserWindow::requestThumbnails);
    connect(model_, &ImageDirectoryModel::scanned, this, &ImageBrowserWindow::listingReady);
    connect(list_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        if (!scanning_ && current.isValid()) selectRow(current.row());
    });
    connect(list_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { thumbnailsTimer_.start(); });
    connect(canvas_, &ImageCanvas::zoomChanged, this, &ImageBrowserWindow::updateStatus);
    connect(canvas_, &ImageCanvas::viewportChanged, this, &ImageBrowserWindow::updateStatus);
    connect(canvas_, &ImageCanvas::cropAccepted, this, &ImageBrowserWindow::applyEditRegion);
    connect(canvas_, &ImageCanvas::cropCancelled, this, [this] { showStatus(QString()); updateActions(); });
    connect(canvas_, &ImageCanvas::navigate, this, [this](int delta) { selectRow(model_->indexOf(document_.path) + delta); });
    createEditorPanel();
    applyDpi();
    screenshotDirectory();
    updateActions();
}

ImageBrowserWindow::~ImageBrowserWindow() {
    loadGeneration_->store(++imageVersion_);
    stopMovie();
    if (busyHintShown_) QApplication::restoreOverrideCursor();
}

void ImageBrowserWindow::createEditorPanel() {
    editorPanel_ = new ImageEditorPanel(this);
    connect(editorPanel_, &ImageEditorPanel::closeRequested, this, [this] {
        editorPanel_->hide(); editorAction_->setChecked(false);
        positionEditorPanel();
        canvas_->setCropping(false); showStatus({}); updateActions();
    });
    connect(editorPanel_, &ImageEditorPanel::toolChanged, this, [this] {
        dispatchAction([this] {
            colorSession_ = false; colorBase_ = {}; editorPanel_->resetColorParameters();
            canvas_->setCropping(false); regionFromPanel_ = false; showStatus({}); positionEditorPanel(); updateActions();
        });
    });
    colorTimer_.setSingleShot(true);
    colorTimer_.setInterval(100);
    connect(&colorTimer_, &QTimer::timeout, this, &ImageBrowserWindow::renderColor);
    connect(editorPanel_, &ImageEditorPanel::colorChanged, this, &ImageBrowserWindow::queueColor);
    connect(editorPanel_, &ImageEditorPanel::drawingStyleChanged, this, [this] {
        canvas_->setAnnotation(editorPanel_->drawingActive() ? editorPanel_->currentOperation() : EditOperation{},
            !busy_ && editorPanel_->isVisible() && editorPanel_->drawingActive());
    });
    connect(canvas_, &ImageCanvas::annotationAccepted, this, [this](const EditOperation& operation) {
        if (editorPanel_->isVisible() && editorPanel_->drawingActive() && editorPanel_->validateOperation(operation)) edit(operation);
    });
    connect(editorPanel_, &ImageEditorPanel::editRequested, this, [this](const EditOperation& operation, bool region) {
        if (region) selectEditRegion(operation);
        else edit(operation);
    });
    connect(editorPanel_, &ImageEditorPanel::applySelectionRequested, this, [this] { applyEditRegion(canvas_->cropRect()); });
    connect(editorPanel_, &ImageEditorPanel::cancelSelectionRequested, this, [this] {
        canvas_->setCropping(false); showStatus({}); updateActions();
    });
    connect(canvas_, &ImageCanvas::selectionChanged, this, [this] {
        editorPanel_->setSelectionState(regionFromPanel_ && canvas_->cropping(), canvas_->cropRect());
    });
    editorPanel_->hide();
}

void ImageBrowserWindow::positionEditorPanel() {
    if (!editorPanel_) return;
    const int gap = qRound(8 * scaleFactor_);
    const int top = toolbar_->geometry().bottom() + gap;
    editorPanel_->setMetrics(scaleFactor_, qMax(1, statusBar()->geometry().top() - top - gap));
    editorPanel_->move(gap, top);
    editorPanel_->raise();
    const int inset = editorPanel_->isVisible()
        ? qMax(0, canvas_->mapFrom(this, QPoint(editorPanel_->geometry().right() + gap, 0)).x()) : 0;
    canvas_->setLeftInset(inset);
}

void ImageBrowserWindow::selectEditRegion(const EditOperation& operation) {
    regionOperation_ = operation;
    regionFromPanel_ = true;
    canvas_->setCropping(true);
    showStatus(tr("拖动选择区域，按 Enter 应用，Esc 取消。"));
    updateActions();
}

void ImageBrowserWindow::applyEditRegion(const QRect& rect) {
    if (rect.isEmpty()) return;
    auto operation = regionFromPanel_ ? editorPanel_->currentOperation() : regionOperation_;
    if (regionFromPanel_ && !editorPanel_->validateOperation(operation)) return;
    operation.rect = rect;
    edit(operation);
}

void ImageBrowserWindow::applyDpi(double scale) {
    if (scale <= 0) scale = Util::getScreenScaleFactor(frameGeometry().center());
    scaleFactor_ = scale;
    toolbar_->setProperty("browserExtensionExtent", qRound(22 * scale) + 1);
    // 窗口级样式表是「祖先规则」，会盖掉全局 qss 里任何同属性的声明（不论特异性）。所以 caption
    // 的 12px 只能写在这里，用同级的 #browser* 选择器把它从上面那条 QWidget 规则里抢回来。
    setStyleSheet(QStringLiteral(
        "#ImageBrowserWindow, #ImageBrowserWindow QWidget { font-size: %1px; }"
        "#ImageBrowserWindow QLabel#browserTitleLabel,"
        "#ImageBrowserWindow QLabel#browserImageCounter,"
        "#ImageBrowserWindow QLabel#browserImageDetails,"
        "#ImageBrowserWindow QLabel#browserStatusMessage { font-size: %2px; }")
        .arg(qRound(13 * scale)).arg(qRound(12 * scale)));
    refreshVisualStyle();
    toolbar_->setIconSize(QSize(qRound(20 * scale), qRound(20 * scale)));
    positionEditorPanel();
    const int barHeight = qRound(32 * scale);
    for (auto* a : toolbar_->actions()) {
        auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(a));
        if (!button) continue;
        if (button->toolButtonStyle() == Qt::ToolButtonIconOnly) {
            button->setFixedSize(qRound(30 * scale), barHeight);
        } else {
            // 图标 + 文字的按钮宽度交给 sizeHint（中文文案长度不定），只锁高度，否则文字会被裁掉。
            button->setMinimumWidth(0);
            button->setFixedHeight(barHeight);
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
    }
    if (auto* extension = toolbar_->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"))) {
        extension->setFixedSize(qRound(22 * scale), barHeight);
        extension->setStyleSheet(QStringLiteral("padding: 1px; min-width: 0; min-height: 0; border: 0;"));
        extension->setIcon(ThemeIcon::icon("browser-more.png"));
    }
    const int controlSide = qRound(28 * scale);
    for (auto* button : std::as_const(controlButtons_)) button->setFixedSize(controlSide, controlSide);
    zoomButton_->setFixedHeight(controlSide);
    zoomButton_->setMinimumWidth(qRound(48 * scale));
    zoomButton_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    toolPill_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (auto* row = qobject_cast<QHBoxLayout*>(toolPill_->layout())) {
        row->setSpacing(qRound(4 * scale));
        row->setContentsMargins(qRound(6 * scale), qRound(4 * scale), qRound(6 * scale), qRound(4 * scale));
    }
    for (auto* line : toolPill_->findChildren<QFrame*>(QStringLiteral("browserToolPillSeparator")))
        line->setFixedSize(qRound(1 * scale), qRound(16 * scale));
    for (auto* line : controlRight_->findChildren<QFrame*>(QStringLiteral("browserControlSeparator")))
        line->setFixedSize(qRound(1 * scale), qRound(16 * scale));
    details_->setMaximumWidth(qRound(220 * scale));
    statusLabel_->setMaximumWidth(qRound(280 * scale));
    for (QHBoxLayout* row : {qobject_cast<QHBoxLayout*>(controlLeft_->layout()), qobject_cast<QHBoxLayout*>(controlRight_->layout())})
        if (row) row->setSpacing(qRound(2 * scale));
    if (auto* row = qobject_cast<QHBoxLayout*>(controlCopySave_->layout()))
        row->setSpacing(qRound(2 * scale));
    if (auto* row = qobject_cast<QHBoxLayout*>(controlBar_->layout())) {
        row->setContentsMargins(qRound(8 * scale), 0, qRound(8 * scale), 0);
        // 左右两侧等宽，保证工具胶囊相对整个窗口居中。
        row->setSpacing(qRound(12 * scale));
    }
    // 高度必须固定：底栏内容一变就会顶动状态栏高度，中央部件跟着 resize，
    // 又反过来触发 updateStatus()，形成无限重排。
    controlBar_->setFixedHeight(qRound(38 * scale));
    const int overlay = qRound(38 * scale);
    for (QToolButton* button : {overlayPrevious_, overlayNext_}) {
        button->setIconSize(QSize(qRound(24 * scale), qRound(24 * scale)));
        button->setFixedSize(overlay, overlay);
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
    positionOverlays();
    layoutControlBar();
    positionTitle();
    QTimer::singleShot(0, this, [this, scale] {
        splitter_->setSizes({qRound(144 * scale), qMax(1, splitter_->width() - qRound(144 * scale))});
        layoutControlBar();
        positionTitle();
    });
    thumbnailsTimer_.start();
}

void ImageBrowserWindow::styleToolButtons() {
    // 工具栏自己的样式表只能匹配 QToolBar：一旦写成 "QToolBar QToolButton"，它就成了子按钮的
    // 祖先规则，无论全局 qss 的特异性多高都压不过它，主按钮的蓝色底永远不会出现。
    toolbar_->setStyleSheet(QStringLiteral(
        "QToolBar { spacing: 0; border: 0; border-bottom: 1px solid %1; padding: %2px %3px; }")
        .arg(ThemeManager::tokens().border.name(QColor::HexRgb))
        .arg(qRound(3 * scaleFactor_)).arg(qRound(6 * scaleFactor_)));
    // 按钮文案会被 action 的 text 覆盖回去（action 的 icon 一变就重置），所以每次都要重贴。
    for (auto* button : findChildren<QToolButton*>()) {
        const QString text = button->property("browserText").toString();
        if (!text.isEmpty()) button->setText(text);
    }
}

void ImageBrowserWindow::setBusy(bool busy, const QString& message) {
    busy_ = busy;
    busyMessage_ = message;
    if (busy) busyCursorTimer_.start(1000);
    else {
        rendering_ = false;
        setProperty("imageProcessing", false);
        busyCursorTimer_.stop();
        if (busyHintShown_) { QApplication::restoreOverrideCursor(); busyHintShown_ = false; }
        if (editorPanel_) editorPanel_->setBusy(false);
    }
    updateActions();
    if (!busy_) {
        while (!deferredActions_.isEmpty() && !busy_ && !loading_) {
            auto callback = deferredActions_.takeFirst();
            QPointer<ImageBrowserWindow> guard(this);
            callback();
            if (!guard) return;
        }
    }
}

void ImageBrowserWindow::dispatchAction(std::function<void()> action) {
    if (rendering_ || colorPending_ || colorRendering_) deferredActions_.append(std::move(action));
    else if (!busy_) action();
}

void ImageBrowserWindow::showStatus(const QString& message, int timeout) {
    if (statusText_ == message && timeout == 0) return;
    statusText_ = message;
    statusTimer_.stop();
    if (timeout > 0 && !message.isEmpty()) statusTimer_.start(timeout);
    // 提示的出现/消失会改变左组宽度，必须重排。这条调用不能放进 updateStatus()：
    // updateStatus 也会被标签的 Resize 回调触发，两边互相触发就是死循环。
    layoutControlBar();
    updateStatus();
}

QToolButton* ImageBrowserWindow::controlButton(QAction* action) {
    auto* button = new QToolButton(this);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setDefaultAction(action);
    controlButtons_ << button;
    return button;
}

void ImageBrowserWindow::syncControlButtons() {
    // setDefaultAction 会同步 enabled / checked / icon / text，但不会跟随 action 的可见性，
    // 应用裁剪、取消裁剪、播放这几个条件按钮要靠这里才真的出现和消失。
    for (auto* button : std::as_const(controlButtons_)) {
        if (auto* action = button->defaultAction()) button->setVisible(action->isVisible());
    }
}

void ImageBrowserWindow::setZoomStep(double factor) {
    if (document_.current.isNull()) return;
    canvas_->setZoom(canvas_->zoom() * factor);
}

void ImageBrowserWindow::positionTitle() {
    if (!titleLabel_ || !toolbar_) return;
    if (titleText_.isEmpty()) { titleLabel_->hide(); return; }
    titleLabel_->show();
    // 可用区间就是弹性空档占据的那一段：它左边是左侧按钮组，右边是右侧按钮组。
    QRect band = toolbar_->rect();
    if (auto* spacer = spacerAction_ ? toolbar_->widgetForAction(spacerAction_) : nullptr;
        spacer && spacer->isVisible() && spacer->geometry().width() > 0) {
        band = spacer->geometry();
    }
    const int margin = qRound(8 * scaleFactor_);
    const QFontMetrics metrics = titleLabel_->fontMetrics();
    const int available = qMax(0, band.width() - 2 * margin);
    const int width = qMin(available, metrics.horizontalAdvance(titleText_) + 2);
    if (width < qRound(60 * scaleFactor_)) { titleLabel_->hide(); return; }
    const int x = qBound(band.left() + margin, (toolbar_->width() - width) / 2, band.right() - margin - width);
    titleLabel_->setGeometry(x, 0, width, toolbar_->height());
    titleLabel_->setText(metrics.elidedText(titleText_, Qt::ElideMiddle, width));
    titleLabel_->raise();
}

void ImageBrowserWindow::positionOverlays() {
    if (!canvas_ || !overlayPrevious_) return;
    const int size = overlayPrevious_->width();
    const int margin = qRound(12 * scaleFactor_);
    const int y = (canvas_->height() - size) / 2;
    overlayPrevious_->move(margin, y);
    overlayNext_->move(qMax(margin, canvas_->width() - margin - size), y);
}

void ImageBrowserWindow::updateOverlays() {
    if (!canvas_ || !overlayPrevious_) return;
    const int row = model_->indexOf(document_.path);
    // underMouse() 也算上箭头自己：鼠标从画布移到箭头上时画布会先收到 Leave，
    // 只看画布的话箭头会在指针刚碰到它的一瞬间闪掉。
    const bool hovered = canvas_->underMouse() || overlayPrevious_->underMouse() || overlayNext_->underMouse();
    const bool visible = hovered && !canvas_->cropping() && !busy_ && !loading_ && !document_.current.isNull();
    overlayPrevious_->setVisible(visible && row > 0);
    overlayNext_->setVisible(visible && row >= 0 && row + 1 < model_->rowCount());
    if (overlayPrevious_->isVisible() || overlayNext_->isVisible()) positionOverlays();
}

void ImageBrowserWindow::layoutControlBar() {
    if (!controlBar_ || controlBar_->width() <= 0) return;
    // 逐档收起：文件信息 → 工具胶囊 → 复制/保存。窄到 560px 时仍保住翻页、缩放、全屏。
    auto apply = [this](int level) {
        details_->setVisible(level < 1 && hasDetails_);
        toolPill_->setVisible(level < 2);
        // 状态提示只是短时 toast，窄窗口里先让位；它不进 measure()，否则每来一条提示都会挤动胶囊。
        statusLabel_->setVisible(level < 2 && !statusText_.isEmpty());
        controlCopySave_->setVisible(level < 3);
    };
    // 用「候选档位」而不是控件的 isVisible() 来量宽度：任何祖先隐藏时 isVisible() 都是 false，
    // 会让底栏的取舍跟着窗口显隐变化，进而在重排回调里来回抖动。
    auto groupWidth = [this](QWidget* group, int level) {
        auto* row = qobject_cast<QHBoxLayout*>(group->layout());
        const auto margins = row->contentsMargins();
        int width = margins.left()+margins.right(), count = 0;
        for (int i = 0; i < row->count(); ++i) {
            auto* widget = row->itemAt(i)->widget();
            if (!widget || widget == statusLabel_) continue;
            if (widget == details_ && (level >= 1 || !hasDetails_)) continue;
            if (widget == controlCopySave_ && level >= 3) continue;
            if (auto* button = qobject_cast<QToolButton*>(widget))
                if (button->defaultAction() && !button->defaultAction()->isVisible()) continue;
            width += qBound(widget->minimumWidth(), widget->sizeHint().width(), widget->maximumWidth());
            ++count;
        }
        return width + qMax(0, count-1)*row->spacing();
    };
    auto sideWidth = [this, &groupWidth](int level) {
        return qMax(groupWidth(controlLeft_, level), groupWidth(controlRight_, level));
    };
    auto measure = [this, &sideWidth](int level) {
        const auto margins = controlBar_->layout()->contentsMargins();
        int width = margins.left() + margins.right() + controlBar_->layout()->spacing() * (level < 2 ? 2 : 1);
        width += 2 * sideWidth(level);
        if (level < 2) width += toolPill_->sizeHint().width();
        return width;
    };
    int level = 3;
    for (int candidate = 0; candidate <= 3; ++candidate) {
        if (measure(candidate) <= controlBar_->width()) { level = candidate; break; }
    }
    apply(level);
    const int minimumSide = sideWidth(level);
    controlLeft_->setMinimumWidth(minimumSide);
    controlRight_->setMinimumWidth(minimumSide);
    controlLevel_ = level;
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
void ImageBrowserWindow::refresh(const QString& selectPath) {
    if (busy_ || scanning_) { refreshTimer_.start(); return; }
    pendingSelection_ = selectPath.isEmpty() ? document_.path : selectPath;
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
        else if (!document_.sourceUnchanged()) showStatus(tr("原文件已被外部修改，请将编辑结果另存为新文件。"));
    } else if (!document_.current.isNull()) showStatus(tr("原文件已不在当前目录中，仍可另存图片副本。"));
    thumbnailsTimer_.start();
    updateActions();
}
void ImageBrowserWindow::selectRow(int row) {
    if (rendering_ || colorPending_ || colorRendering_) {
        dispatchAction([this, row] { selectRow(row); }); return;
    }
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
    deferredActions_.clear();
    colorSession_ = false; colorBase_ = {}; editorPanel_->resetColorParameters();
    stopMovie();
    // 切换图片时画布上闪一下“正在加载图片”比空白更烦，只有画布本来就空着（首次打开、
    // 上一张打不开）才提示。
    const bool blankCanvas = document_.current.isNull();
    const quint64 version = ++imageVersion_;
    loadGeneration_->store(version);
    loading_ = true;
    animationSource_ = false;
    document_ = {};
    document_.path = path;
    displayedSize_ = {};
    canvas_->setImage({}, true);
    if (blankCanvas) canvas_->setMessage(tr("正在加载图片…"));
    showStatus(QString());
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
        if (result.limited) showStatus(tr("受限预览：超过 7200 万像素的图片不支持编辑或复制。"));
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
                showStatus(tr("无法播放动图，当前显示静态帧。"));
                stopMovie(); updateActions();
            });
            if (isVisible()) movie_->start();
            else resumeMovie_ = true;
        } else if ((QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0 ||
                    QFileInfo(path).suffix().compare("webp", Qt::CaseInsensitive) == 0) && !result.animationSupported) {
            showStatus(tr("当前图片插件不支持动图播放。"));
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
    setBusy(true);
    loader_->run(this, [path, frameNumber] { return ImageDocument::readFrame(path, frameNumber); },
                 [this, after](const ImageResult& result) {
        setBusy(false);
        if (!result.error.isEmpty()) showStatus(result.error);
        else {
            stopMovie();
            document_.original = document_.current = result.image;
            document_.operations.clear();
            document_.position = 0;
            document_.frozenFrame = true;
            displayedSize_ = result.image.size();
            canvas_->setImage(result.image, true);
            showStatus(tr("正在编辑静态帧，请另存为 PNG 或 JPEG 图片。"));
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
    // tooltip 必须是「尺寸 + 文件大小」，既不能带文件名也不能带目录路径（测试按这个断言）。
    const QString caption = name.isEmpty() ? QString()
        : QStringLiteral("%1 x %2   %3").arg(qMax(0, displayedSize_.width())).arg(qMax(0, displayedSize_.height())).arg(fileSize);
    details_->setToolTip(caption);
    // 按 maximumWidth 截断，不按当前宽度：标签是 Preferred 策略，用「已经截短的文本」再去算
    // sizeHint 会越缩越短，同一段文字在深浅两套主题下能截出不同的长度。
    details_->setText(details_->fontMetrics().elidedText(caption, Qt::ElideMiddle, details_->maximumWidth()));
    // 显隐统一由 layoutControlBar 决定：这里再 setVisible 一次会和它互相打架，导致无限重排。
    hasDetails_ = !caption.isEmpty();
    counterLabel_->setText(row < 0 || name.isEmpty() ? QString() : QStringLiteral("%1 / %2").arg(row + 1).arg(model_->rowCount()));
    statusLabel_->setText(statusLabel_->fontMetrics().elidedText(statusText_, Qt::ElideRight, statusLabel_->maximumWidth()));
    statusLabel_->setToolTip(statusText_);
    zoomButton_->setText(QStringLiteral("%1%").arg(qRound(canvas_->zoom() * 100)));
    titleText_ = name;
    positionTitle();
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
    if (rendering_ || colorPending_ || colorRendering_) {
        updateEditorState();
        return;
    }
    const bool available = !document_.current.isNull() && !loading_ && !busy_;
    const bool editable = available && !document_.limited && !movie_;
    wallpaperAction_->setEnabled(editable && SystemBackground::supported());
    lockScreenAction_->setEnabled(editable && SystemBackground::supported());
    editorAction_->setEnabled(editable);
    updateEditorState();
    for (auto* action : editActions_) action->setEnabled(editable);
    for (auto* action : navigationActions_) action->setEnabled(!busy_);
    const int row = model_->indexOf(document_.path);
    previousAction_->setEnabled(!busy_ && row > 0);
    nextAction_->setEnabled(!busy_ && row >= 0 && row + 1 < model_->rowCount());
    saveAction_->setEnabled(available && !document_.limited && !movie_);
    copyAction_->setEnabled(available && !document_.limited);
    deleteAction_->setEnabled(available && !document_.path.isEmpty());
    convertAction_->setEnabled(available && !document_.limited);
    overwriteAction_->setEnabled(editable && !animationSource_ && document_.sourceUnchanged() &&
        (QFileInfo(document_.path).suffix().toLower() == "png" || QFileInfo(document_.path).suffix().toLower() == "jpg" || QFileInfo(document_.path).suffix().toLower() == "jpeg"));
    undoAction_->setEnabled(editable && document_.canUndo());
    redoAction_->setEnabled(editable && document_.canRedo());
    resetAction_->setEnabled(editable && document_.canUndo());
    playAction_->setVisible(movie_ != nullptr);
    playAction_->setEnabled(movie_ && !busy_);
    const QString playIcon = movie_ && movie_->state() == QMovie::Running ? "browser-pause.png" : "browser-play.png";
    if (playAction_->property("browserIcon").toString() != playIcon) {
        playAction_->setProperty("browserIcon", playIcon);
        playAction_->setIcon(ThemeIcon::icon(playIcon));
    }
    frameAction_->setVisible(movie_ != nullptr);
    frameAction_->setEnabled(movie_ && available);
    zoomInAction_->setEnabled(available);
    zoomOutAction_->setEnabled(available);
    fullscreenAction_->setChecked(isFullScreen());
    cropAction_->setChecked(canvas_->cropping());
    exitFullscreenAction_->setEnabled(isFullScreen() || canvas_->cropping() || editorPanel_->isVisible());
    applyCropAction_->setText(regionOperation_.kind == EditOperation::Crop ? tr("应用裁剪") : tr("应用编辑"));
    cancelCropAction_->setText(regionOperation_.kind == EditOperation::Crop ? tr("取消裁剪") : tr("取消编辑"));
    applyCropAction_->setVisible(canvas_->cropping());
    cancelCropAction_->setVisible(canvas_->cropping());
    applyCropAction_->setEnabled(editable);
    cancelCropAction_->setEnabled(!busy_);
    list_->setEnabled(!busy_ && !scanning_);
    syncControlButtons();
    // 条件按钮（应用裁剪 / 播放）刚显隐会改变两端的尺寸，重排一次再定位标题。
    updateStatus();
    layoutControlBar();
    updateOverlays();
}

void ImageBrowserWindow::updateEditorState() {
    const bool processing = rendering_ || colorPending_ || colorRendering_;
    const bool editable = !document_.current.isNull() && !loading_ && !document_.limited && !movie_ && (!busy_ || processing);
    editorPanel_->setDocumentContext(document_.path, document_.current.size(), editable, processing);
    editorPanel_->setSelectionState(regionFromPanel_ && canvas_->cropping(), canvas_->cropRect());
    canvas_->setAnnotation(editorPanel_->drawingActive() ? editorPanel_->currentOperation() : EditOperation{},
        !busy_ && editorPanel_->isVisible() && editorPanel_->drawingActive());
}

void ImageBrowserWindow::refreshVisualStyle() {
    const auto& theme = ThemeManager::tokens();
    const QString key = theme.border.name() + theme.textPrimary.name() + theme.accent.name() + QString::number(scaleFactor_);
    if (visualStyleKey_ == key) return;
    visualStyleKey_ = key;
    for (auto* action : findChildren<QAction*>()) {
        const QString icon = action->property("browserIcon").toString();
        if (!icon.isEmpty()) action->setIcon(ThemeIcon::icon(icon));
    }
    if (auto* extension = toolbar_->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button")))
        extension->setIcon(ThemeIcon::icon("browser-more.png"));
    styleToolButtons();
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
bool ImageBrowserWindow::save(bool overwrite, bool refreshListing, const QByteArray& requestedFormat,
                              const ImageConversionOptions& options) {
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
    setBusy(true);
    SaveProgress progress(this);
    auto result = std::make_shared<SaveResult>();
    const QImage image = document_.current;
    const QDateTime expectedModified = overwrite ? document_.modified : QDateTime();
    const qint64 expectedSize = document_.fileSize;
    loader_->run(&progress, [path, image, format, expectedModified, expectedSize, options] {
        return ImageDocument::save(path, image, format, expectedModified, expectedSize, options);
    },
                 [&progress, result](const SaveResult& saved) { *result = saved; progress.accept(); });
    progress.exec();
    setBusy(false);
    if (!result->success) {
        QMessageBox::warning(this, tr("保存失败"), result->error);
        updateActions();
        return false;
    }
    if (format == "pdf") {
        // PDF is an export: retain the source image, selection and undo history.
        showStatus(tr("PDF 已保存"), 3000);
        updateActions();
        return true;
    }
    ImageResult saved;
    saved.image = result->image;
    saved.modified = QFileInfo(path).lastModified();
    saved.fileSize = QFileInfo(path).size();
    document_.load(path, saved);
    colorSession_ = false; colorBase_ = {}; editorPanel_->resetColorParameters();
    animationSource_ = false;
    displayedSize_ = saved.image.size();
    showStatus(tr("图片已保存"), 3000);
    if (refreshListing) {
        if (QFileInfo(path).absolutePath() != directory_) openDirectory(QFileInfo(path).absolutePath(), path);
        else { loadImage(path); refresh(); }
    }
    updateActions();
    return true;
}
void ImageBrowserWindow::setSystemBackground(bool lockScreen) {
    if (busy_ || loading_ || movie_ || document_.limited || document_.current.isNull() || !SystemBackground::supported()) return;
    setBusy(true, lockScreen ? tr("正在设置锁屏背景…") : tr("正在设置桌面背景…"));
    loader_->run(this, [image = document_.current, lockScreen] {
        return SystemBackground::apply(image, lockScreen ? SystemBackground::Target::LockScreen : SystemBackground::Target::Desktop);
    }, [this, lockScreen](const SystemBackground::Result& result) {
        setBusy(false);
        if (result.success) showStatus(lockScreen ? tr("已设为锁屏背景") : tr("已设为桌面背景"), 5000);
        else showStatus(result.error);
    });
}

void ImageBrowserWindow::convertFormat() {
    if (busy_ || loading_ || document_.current.isNull() || document_.limited) return;
    if (movie_) {
        if (QMessageBox::question(this, tr("格式转换"), tr("将当前帧转换为静态图片，不保留动画，是否继续？"),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        freezeFrame([this] { convertFormat(); });
        return;
    }
    ImageConversionDialog dialog(document_.current.size(), this);
    if (dialog.exec() == QDialog::Accepted) save(false, true, dialog.format(), dialog.options());
}

void ImageBrowserWindow::deleteImage() {
    if (busy_ || loading_ || scanning_ || document_.current.isNull() || document_.path.isEmpty()) return;
    const QString path = document_.path;
    const QString name = QFileInfo(path).fileName();
    const QString question = document_.dirty() ? tr("删除「%1」？未保存的编辑会一并丢弃。").arg(name)
                                               : tr("删除「%1」？").arg(name);
    if (QMessageBox::question(this, tr("删除图片"), question,
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    // 先算好邻居：优先下面那张（删除后它正好补位），到底了再退回上面那张。
    const int row = model_->indexOf(path);
    QString next;
    if (row >= 0) {
        if (row + 1 < model_->rowCount()) next = model_->pathAt(row + 1);
        else if (row > 0) next = model_->pathAt(row - 1);
    }
    QString error;
    if (!QFile::moveToTrash(path, &error)) {
        QMessageBox::warning(this, tr("删除图片"), tr("无法删除「%1」。\n%2").arg(
            name, error.isEmpty() ? tr("文件可能正被占用。") : error));
        return;
    }
    // 先清空文档再刷新，否则 listingReady 会把这个已经不存在的路径又选中一次。
    loadGeneration_->store(++imageVersion_);
    stopMovie();
    document_ = {};
    displayedSize_ = {};
    animationSource_ = false;
    canvas_->setImage({}, true);
    showStatus(tr("已删除「%1」").arg(name), 3000);
    refresh(next);
}

void ImageBrowserWindow::showContextMenu(const QPoint& globalPosition) {
    updateActions();
    QMenu menu(this);
    for (auto* action : toolbar_->actions()) {
        if (action == spacerAction_) continue;   // 弹性空档只是布局占位，不是菜单项
        if (action->isSeparator()) menu.addSeparator();
        else if (action->menu()) {
            menu.addSeparator();
            menu.addActions(action->menu()->actions());
        } else if (action->isVisible()) menu.addAction(action);
    }
    // 「截图目录」已从顶栏按钮移到窗口级 action，右键菜单要把这一条补回来。
    menu.addAction(screenshotDirectoryAction_);
    // 编辑工具已经搬到底部胶囊里，右键菜单要把它们补回来。
    menu.addSeparator();
    for (auto* action : std::as_const(editActions_)) if (action->isVisible()) menu.addAction(action);
    for (auto* action : {undoAction_, redoAction_, resetAction_, copyAction_, saveAction_})
        if (action->isVisible()) menu.addAction(action);
    // 删除是唯一会动磁盘文件的菜单项，用一条分隔线单独隔在底部。
    if (deleteAction_->isEnabled()) { menu.addSeparator(); menu.addAction(deleteAction_); }
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
    renderDocument(candidate, false, true);
}
void ImageBrowserWindow::queueColor(const EditOperation& operation) {
    if (loading_ || document_.limited || movie_ || document_.current.isNull() ||
        (busy_ && !colorRendering_ && !colorPending_)) return;
    if (!colorSession_) {
        colorBase_ = document_;
        colorSession_ = true;
    }
    pendingColor_ = operation;
    colorPending_ = true;
    setProperty("imageProcessing", true);
    if (!colorRendering_ && !colorTimer_.isActive()) colorTimer_.start();
    setBusy(true, tr("正在处理图片…"));
}

void ImageBrowserWindow::renderColor() {
    if (!colorPending_ || colorRendering_) return;
    auto candidate = colorBase_;
    const auto& op = pendingColor_;
    if (op.brightness || op.contrast || op.saturation || op.temperature || op.tint || op.opacity != 100)
        candidate.append(op);
    colorPending_ = false;
    colorRendering_ = true;
    renderDocument(candidate, true);
}

void ImageBrowserWindow::renderDocument(ImageDocument candidate, bool liveColor, bool incremental) {
    if (busy_ && !liveColor) return;
    if (!liveColor) {
        colorSession_ = false; colorBase_ = {}; editorPanel_->resetColorParameters();
    }
    canvas_->setCropping(false);
    rendering_ = true;
    setProperty("imageProcessing", true);
    const quint64 version = ++imageVersion_;
    setBusy(true, tr("正在处理图片…"));
    // Each preview starts at the session's unadjusted image. Never stack slider
    // values on the previous preview or replay unrelated expensive operations.
    const QImage base = liveColor ? colorBase_.current : incremental ? document_.current : candidate.original;
    const QVector<EditOperation> operations = liveColor
        ? (candidate.position > colorBase_.position ? QVector<EditOperation>{pendingColor_} : QVector<EditOperation>{})
        : incremental ? QVector<EditOperation>{candidate.operations[candidate.position-1]} : candidate.operations;
    const int count = liveColor || incremental ? operations.size() : candidate.position;
    loader_->run(this, [base, operations, count] {
        return ImageDocument::render(base, operations, count);
    }, [this, candidate, version, liveColor](const ImageResult& result) mutable {
        if (version != imageVersion_) return;
        if (liveColor) colorRendering_ = false;
        if (!result.error.isEmpty()) {
            canvas_->setImage(document_.current);
            showStatus(result.error);
        }
        else {
            const bool sizeChanged = result.image.size() != document_.current.size();
            candidate.current = result.image;
            document_ = candidate;
            displayedSize_ = result.image.size();
            // Keep zoom and pan while drawing consecutive annotations or adjusting color.
            canvas_->setImage(result.image, sizeChanged);
            showStatus(QString());
        }
        if (liveColor && colorPending_) {
            colorTimer_.start();
            updateActions();
        } else setBusy(false);
    });
}
void ImageBrowserWindow::resizeImage() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("调整图片尺寸"));
    auto* form = new QFormLayout(&dialog);
    auto* width = new EditorValueSpinBox(&dialog);
    auto* height = new EditorValueSpinBox(&dialog);
    width->setRange(1, int(MaxEditablePixels));
    height->setRange(1, int(MaxEditablePixels));
    width->setValue(document_.current.width());
    height->setValue(document_.current.height());
    auto* lock = new QCheckBox(tr("锁定宽高比"), &dialog);
    lock->setChecked(true);
    const double ratio = double(document_.current.width()) / document_.current.height();
    connect(width, &QSpinBox::valueChanged, &dialog, [height, lock, ratio](int value) {
        if (lock->isChecked()) { const QSignalBlocker blocker(height); height->setValue(qRound(qBound(1.0, value / ratio, double(MaxEditablePixels)))); }
    });
    connect(height, &QSpinBox::valueChanged, &dialog, [width, lock, ratio](int value) {
        if (lock->isChecked()) { const QSignalBlocker blocker(width); width->setValue(qRound(qBound(1.0, value * ratio, double(MaxEditablePixels)))); }
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
    if (directory.isEmpty()) { showStatus(tr("请拖入本地图片或目录。"), 4000); return; }
    if (!discardOrSave()) return;
    openDirectory(directory, selection);
    if (urls.size() > 1) showStatus(tr("已打开第一个有效项目，忽略了 %1 项。").arg(urls.size() - 1), 6000);
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
    if (event->type() == QEvent::PaletteChange && details_) { refreshVisualStyle(); updateActions(); update(); list_->viewport()->update(); }
}
bool ImageBrowserWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == list_->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        thumbnailsTimer_.start();
    if (watched == canvas_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        positionOverlays(); updateOverlays();
        QTimer::singleShot(0, this, &ImageBrowserWindow::positionEditorPanel);
    }
    if (watched == controlBar_ && event->type() == QEvent::Resize) layoutControlBar();
    if (watched == toolbar_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        QTimer::singleShot(0, this, [this] { positionTitle(); positionEditorPanel(); });
    // 悬停显形：Enter / Leave 都要判一次，且箭头自己也算「还在悬停」，否则指针碰到箭头会闪。
    if ((watched == canvas_ || watched == overlayPrevious_ || watched == overlayNext_) &&
        (event->type() == QEvent::Enter || event->type() == QEvent::Leave))
        QTimer::singleShot(0, this, &ImageBrowserWindow::updateOverlays);
    return QMainWindow::eventFilter(watched, event);
}
} // namespace ImageBrowser
