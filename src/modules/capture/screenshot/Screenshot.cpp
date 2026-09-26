#include "Screenshot.h"
#include "modules/capture/annotation/DrawPanel.h"
#include "modules/capture/screenshot/ScreenshotActionController.h"
#include <QApplication>
#include <QMouseEvent>
#include <QFileDialog>
#include <QClipboard>
#include <QDateTime>
#include <QPainter>
#include <QImage>
#include <QScreen>
#include <QCursor>
#include <QMutex>
#include <QMenu>
#include <QPen>
#include <QBrush>
#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QThread>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

#include <opencv2/opencv.hpp>

#include "core/imaging/detector/RectDetector.h"
#include "core/imaging/CodeScanner.h"
#include "core/imaging/ImageEnhance.h"
#include "core/imaging/ImageUtil.h"
#include "core/imaging/SmartMask.h"
#include "core/network/PaddleOcrClient.h"
#include "core/foundation/Constants.h"
#include "core/platform/Util.h"
#include "modules/capture/screenshot/Amplifier.h"
#include "modules/capture/pin/PinWidget.h"
#include "app/WindowManager.h"
#include "core/settings/SettingModel.h"

// 默认贴图快捷键
QString PIN_KEY = "F6";
// 标记宽度
static const int MARKERT_WIDTH = 4;
// 边框感知宽度
static int BORDER_ESTHESIA_WIDTH = 6;
// 颜色显示RGB格式
static bool IS_RGB_COLOR = true;
// 背景透明色
QColor BACKGROUND_COLOR(0, 0, 0, 160);

///////////////////////////////////////////////////////////
SelectedScreenSizeWidget::SelectedScreenSizeWidget(QWidget *parent) : QWidget(parent) 
{
	setFixedSize(Style::OERECT_FIXED_SIZE);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    backgroundPixmap_ = std::make_unique<QPixmap>(Style::OERECT_FIXED_SIZE);
	backgroundPixmap_->fill(Style::OERECT_BACKGROUND);
	hide();
}

void SelectedScreenSizeWidget::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.drawPixmap(rect(),*backgroundPixmap_);
	painter.setPen(QPen(QColor(Qt::white)));
	painter.drawText(rect(), Qt::AlignLeft | Qt::AlignBottom, info_);
}

void SelectedScreenSizeWidget::onPositionChanged(int x, int y) {
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	const int& ry = y - height() - 1;
	if (ry < 0) {
		this->raise();
	}
	move(x, ((ry < 0) ? y : ry));
	show();
}

void SelectedScreenSizeWidget::onSizeChanged(int w, int h) {
	info_ = QString("%1 x %2").arg(w).arg(h);
	update();
}

ScreenshotWidget::ScreenshotWidget(WindowManager* windowManager, QWidget *parent)
	: QWidget(parent),
	windowManager_(windowManager),
	currentState_(ScreenState::Exploring),
	darkScreen_(nullptr),
	originScreen_(nullptr),
    windowDetector_(std::make_unique<SystemWindowDetector>()),
    opencvDetector_(std::make_unique<OpenCVDetector>()),
    actionController_(std::make_shared<ScreenshotActionController>(windowManager_, this)),
    useOpenCVMode_(false)
{
    // 初始化时读取记住的模式
    if (windowManager_ && windowManager_->setting()) {
        useOpenCVMode_ = windowManager_->setting()->openCVMode();
    }

	// 初始化鼠标
	initCursor();
    
    // 强制窗口覆盖物理全屏，将物理坐标映射到该窗口的逻辑坐标
    QRect r = getDesktopRect();
    double dpr = this->devicePixelRatioF();
    
#ifdef Q_OS_WIN
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    // 跨屏幕（混合DPI）可能会出现被主屏幕的DPR除偏的情况，所以我们仍然用真实的物理坐标去除以该窗口当前被分配的dpr
    setGeometry(r.x() / dpr, r.y() / dpr, r.width() / dpr, r.height() / dpr);
    show(); // 必须先show，保证HWND已完全初始化且被Qt映射
    
    // 对于 Windows API 强行占满物理全屏：
    HWND hwnd = (HWND)winId();
    SetWindowPos(hwnd, HWND_TOPMOST, r.x(), r.y(), r.width(), r.height(), SWP_SHOWWINDOW);
#else
    // Qt::X11BypassWindowManagerHint 完全绕过 WM，避免任务栏 strut 导致窗口被向下推移
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    setGeometry(r.x() / dpr, r.y() / dpr, r.width() / dpr, r.height() / dpr);
    show();
    activateWindow();
#endif

	// 初始化鼠标放大器
	initAmplifier(getGlobalScreen());
	// 初始化大小感知器
	initMeasureWidget();
    // 初始化绘图面板
    initDrawPanel();
	// 置顶
	onTopMost();
	// 开启鼠标实时追踪
	setMouseTracking(true);
    // 强制焦点，防止放大镜等子窗口抢走键盘事件导致 ESC 失效
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
	// 更新鼠标的位置
    mouseUpdateTimer_ = new QTimer(this);
    mouseUpdateTimer_->setSingleShot(true);
    connect(mouseUpdateTimer_, &QTimer::timeout, this, [this]() {
        updateMouse(true);
    });
    mouseUpdateElapsed_.invalidate();

	QPoint localPos = this->mapFromGlobal(QCursor::pos());
	emit sigCursorPosChanged(localPos.x(), localPos.y());

	// 更新鼠标区域窗口
	updateMouse(true);

    // 统一调用当前探测器的初始化
	activeDetector()->init(getGlobalScreen(), [this]() {
        this->updateMouse(true);
    });

    // 连接 ActionController 的关闭信号
    connect(actionController_.get(), &ScreenshotActionController::sigClose, this, &ScreenshotWidget::sigClose);

	qDebug() << "ScreenshotWidget init";
}

ScreenshotWidget::~ScreenshotWidget(void) 
{
	qDebug() << "ScreenshotWidget destory";
}

void ScreenshotWidget::pin() const
{
	if (currentState_ == ScreenState::Editing) {
        QPixmap pixmap = originScreen_->copy(currentRect_);
        if (drawPanel_) {
            drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
        }
        emit actionController_->onStickerRequested(pixmap, currentRect_.topLeft());
	}
}

void ScreenshotWidget::setPinGlobalKey(const QString &key) const
{
	PIN_KEY = key;
}

void ScreenshotWidget::setRgbColor(bool yes) const
{
	IS_RGB_COLOR = yes;
	if (amplifierTool_) {
		amplifierTool_->setRgbColor(IS_RGB_COLOR);
	}
}

void ScreenshotWidget::setBackgroundColorAlpha(int alpha)
{
    BACKGROUND_COLOR.setAlpha(alpha);
}

void ScreenshotWidget::contextMenuEvent(QContextMenuEvent *e) {
	// 右键菜单
    if (menu_ && currentState_ == ScreenState::Editing) {
	    menu_->exec(cursor().pos());
        e->accept();
    }
}

void ScreenshotWidget::mouseDoubleClickEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
        if (currentState_ == ScreenState::Editing) {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onSaveToClipboardRequested(pixmap);
        }
		emit sigDoubleClick();
		e->accept();
	}
}

// 初始化放大器
void ScreenshotWidget::initAmplifier(std::shared_ptr<QPixmap> originPainting)
{
    amplifierTool_.reset(new AmplifierWidget(originPainting, this));
    connect(this, SIGNAL(sigCursorPosChanged(int, int)), amplifierTool_.get(), SLOT(onPositionChanged(int, int)));
	amplifierTool_->setRgbColor(IS_RGB_COLOR);
    amplifierTool_->setOpenCVMode(useOpenCVMode_);

    // 初始化时直接根据鼠标当前位置设定坐标，避免 show() 时在默认位置（如屏幕左上角或中间）闪现
    QPoint localPos = this->mapFromGlobal(QCursor::pos());
    amplifierTool_->onPositionChanged(localPos.x(), localPos.y());

	amplifierTool_->show();
	amplifierTool_->raise();
}

void ScreenshotWidget::initMeasureWidget(void)
{
	sizeTextPanel_.reset(new SelectedScreenSizeWidget(this));
	sizeTextPanel_->show();
	sizeTextPanel_->raise();
}

// 获取当前屏幕区域
QRect ScreenshotWidget::getDesktopRect(void) 
{
    return Util::desktopRect();
}

const std::shared_ptr<QPixmap>& ScreenshotWidget::getBackgroundScreen(void) {
	if (darkScreen_) {
		return darkScreen_;
	}

	// 获得屏幕原画
	std::shared_ptr<QPixmap> screenPixmap = getGlobalScreen();
	darkScreen_.reset(new QPixmap(*screenPixmap));
	QPainter p(darkScreen_.get());
	p.fillRect(darkScreen_->rect(), BACKGROUND_COLOR);
	return darkScreen_;
}

std::shared_ptr<QPixmap> ScreenshotWidget::getGlobalScreen(void) {
	if (!originScreen_) {
        originScreen_ = std::make_shared<QPixmap>(QPixmap::fromImage(Util::grabDesktopImage()));
        originScreen_->setDevicePixelRatio(1.0);
	}
	return originScreen_;
}

// 设置感知区域
void ScreenshotWidget::setEsthesiaRect(const QRect &rect)
{
	esthesiaRect_ = rect;
	if (!rect.isEmpty()) {
		sizeTextPanel_->onPositionChanged(rect.x(), rect.y());
		sizeTextPanel_->onSizeChanged(rect.width(), rect.height());
	}
}

const QRect& ScreenshotWidget::getEsthesiaRect() const
{
	return esthesiaRect_;
}

IRectDetector* ScreenshotWidget::activeDetector() const
{
    return useOpenCVMode_ ? opencvDetector_.get() : windowDetector_.get();
}

void ScreenshotWidget::onTopMost(void)
{
#ifdef Q_OS_WIN32
    SetWindowPos((HWND)this->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
#else
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::WindowStaysOnTopHint;
	setWindowFlags(flags);
#endif
}

void ScreenshotWidget::onScreenBorderPressed(int x, int y)
{
	if (amplifierTool_->isHidden()) {
		amplifierTool_->move(x, y);
		amplifierTool_->show();
		amplifierTool_->raise();
	}
}

void ScreenshotWidget::onScreenBorderReleased(int x, int y)
{
	if (amplifierTool_->isVisible()) {
		amplifierTool_->hide();
	}
}

void ScreenshotWidget::onSelectedScreenSizeChanged(int w, int h)
{
	sizeTextPanel_->onSizeChanged(w, h);
    
    listMarker_.clear();
	/// 重新计算八个锚点
	// 角点
	listMarker_.push_back(QPoint(currentRect_.x(), currentRect_.y()));
	listMarker_.push_back(QPoint(currentRect_.x() + w - MARKERT_WIDTH, currentRect_.y()));
	listMarker_.push_back(QPoint(currentRect_.x(), currentRect_.y() + h - MARKERT_WIDTH));
	listMarker_.push_back(QPoint(currentRect_.x() + w - MARKERT_WIDTH, currentRect_.y() + h - MARKERT_WIDTH));

	// 中点
	listMarker_.push_back(QPoint(currentRect_.x() + (w >> 1), currentRect_.y()));
	listMarker_.push_back(QPoint(currentRect_.x() + (w >> 1), currentRect_.y() + h - MARKERT_WIDTH));
	listMarker_.push_back(QPoint(currentRect_.x(), currentRect_.y() + (h >> 1)));
	listMarker_.push_back(QPoint(currentRect_.x() + w - MARKERT_WIDTH, currentRect_.y() + (h >> 1) - MARKERT_WIDTH));

    if (drawPanel_) {
        moveDrawPanel();
    }
}

void ScreenshotWidget::onSelectedScreenPosChanged(int x, int y)
{
	sizeTextPanel_->onPositionChanged(x, y);
}

void ScreenshotWidget::initCursor() 
{
    setCursor(Util::multicolorCursor());
}

void ScreenshotWidget::initDrawPanel(void)
{
    if (!drawPanel_) {
        drawPanel_ = std::make_unique<DrawPanel>(windowManager_->setting(), this, this);
        drawPanel_->hide();
        
        connect(drawPanel_.get(), &DrawPanel::sigSticker, this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onStickerRequested(pixmap, currentRect_.topLeft());
        });

        connect(drawPanel_.get(), &DrawPanel::sigAskAi, this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onLLMChatRequested(QString(), pixmap);
        });
        
        connect(drawPanel_.get(), &DrawPanel::sigLongScreenshot, this, [this]() {
            this->onLongScreenshotRequested(currentRect_);
        });

        connect(drawPanel_.get(), &DrawPanel::sigGifRecording, this, [this]() {
            this->onGifRecordingRequested(currentRect_);
        });

        connect(drawPanel_.get(), &DrawPanel::sigOcr,
                this, &ScreenshotWidget::recognizeSelection);
        
        connect(drawPanel_.get(), &DrawPanel::sigSave, this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onSaveRequested(pixmap);
        });
        
        connect(drawPanel_.get(), &DrawPanel::sigFinished, this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onSaveToClipboardRequested(pixmap);
        });

        auto captureSelection = [this]() -> QPixmap {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            return pixmap;
        };

        connect(drawPanel_.get(), &DrawPanel::sigScanCode, this, [this, captureSelection]() {
            if (!drawPanel_) {
                return;
            }
            const QVector<CodeScanResult> results = CodeScanner::scan(captureSelection().toImage());
            if (results.isEmpty()) {
                drawPanel_->showToolMessage(QStringLiteral("未识别到二维码/条码"), true);
                return;
            }

            QStringList parts;
            for (const CodeScanResult& item : results) {
                if (item.type.isEmpty() || item.type == QStringLiteral("QR")) {
                    parts.push_back(item.text);
                } else {
                    parts.push_back(QStringLiteral("[%1] %2").arg(item.type, item.text));
                }
            }
            const QString text = parts.join(QLatin1Char('\n'));
            QApplication::clipboard()->setText(text);
            const QString title = results.size() > 1
                ? QStringLiteral("识别成功（%1 条，已复制）").arg(results.size())
                : QStringLiteral("识别成功（已复制）");
            drawPanel_->showToolMessage(title + QLatin1Char('\n') + text, false);
        });

        connect(drawPanel_.get(), &DrawPanel::sigEnhance, this, [this, captureSelection](int preset) {
            if (!drawPanel_) {
                return;
            }
            QImage src = captureSelection().toImage();
            QImage out = ImageEnhance::apply(src, static_cast<ImageEnhance::Preset>(preset));
            if (out.isNull()) {
                drawPanel_->showToolMessage(QStringLiteral("图像增强失败"), true);
                return;
            }
            drawPanel_->drawer()->replaceWithBitmap(out, currentRect_);
            drawPanel_->showToolMessage(QStringLiteral("已应用图像增强"), false);
        });

        connect(drawPanel_.get(), &DrawPanel::sigSmartMask, this, [this, captureSelection]() {
            if (!drawPanel_) {
                return;
            }
            QImage src = captureSelection().toImage();
            if (SmartMask::detectRegions(src).isEmpty()) {
                drawPanel_->showToolMessage(QStringLiteral("未检测到可打码区域"), true);
                return;
            }
            QImage out = SmartMask::autoMask(src);
            if (out.isNull()) {
                drawPanel_->showToolMessage(QStringLiteral("未检测到可打码区域"), true);
                return;
            }
            drawPanel_->drawer()->replaceWithBitmap(out, currentRect_);
            drawPanel_->showToolMessage(QStringLiteral("已智能打码"), false);
        });

        connect(drawPanel_.get(), &DrawPanel::sigAutoCrop, this, [this, captureSelection]() {
            if (!drawPanel_) {
                return;
            }
            QImage src = captureSelection().toImage();
            const QRect content = ImageUtil::DetectContentRectAdaptive(src);
            if (!content.isValid() || content.size() == src.size()) {
                drawPanel_->showToolMessage(QStringLiteral("无需裁边"), true);
                return;
            }
            QImage cropped = src.copy(content);
            QRect newRect(currentRect_.topLeft() + content.topLeft(), cropped.size());
            const QRect previousRect = currentRect_;
            drawPanel_->drawer()->replaceWithBitmap(cropped, newRect, [this, previousRect]() {
                currentRect_ = previousRect;
                onSelectedScreenSizeChanged(previousRect.width(), previousRect.height());
                moveDrawPanel();
                update();
            });
            currentRect_ = newRect;
            onSelectedScreenSizeChanged(newRect.width(), newRect.height());
            update();
            drawPanel_->showToolMessage(QStringLiteral("已自动裁边"), false);
        });

        connect(drawPanel_.get(), &DrawPanel::sigExtractColors, this, [this, captureSelection]() {
            if (!drawPanel_) {
                return;
            }
            const QVector<QColor> colors = ImageUtil::DominantColors(captureSelection().toImage(), 5);
            if (colors.isEmpty()) {
                drawPanel_->showToolMessage(QStringLiteral("未能提取主色"), true);
                return;
            }
            QStringList hexes;
            for (const QColor& c : colors) {
                hexes.push_back(c.name(QColor::HexRgb).toUpper());
            }
            const QString text = hexes.join(QLatin1Char(' '));
            QApplication::clipboard()->setText(text);
            drawPanel_->showToolMessage(QStringLiteral("主色已复制") + QLatin1Char('\n') + text, false);
        });

        // Initialize menu
        menu_ = new QMenu(this);

        menu_->addAction(QStringLiteral("完成"), this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onSaveToClipboardRequested(pixmap);
        }, QKeySequence("Ctrl+C"));
        
        menu_->addAction(QStringLiteral("保存"), this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onSaveRequested(pixmap);
        }, QKeySequence("Ctrl+S"));
        
        menu_->addAction(QStringLiteral("贴图"), this, [this]() {
            QPixmap pixmap = originScreen_->copy(currentRect_);
            if (drawPanel_) {
                drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
            }
            emit actionController_->onStickerRequested(pixmap, currentRect_.topLeft());
        }, QKeySequence(PIN_KEY));
        
        menu_->addAction(QStringLiteral("撤销"), drawPanel_->drawer(), &Drawer::undo, QKeySequence("Ctrl+Z"));
        
        menu_->addAction(QStringLiteral("截长图"), this, [this]() {
            this->onLongScreenshotRequested(currentRect_);
        });

        menu_->addAction(QStringLiteral("录制 GIF"), this, [this]() {
            this->onGifRecordingRequested(currentRect_);
        });

        if (windowManager_->setting()->paddleOcrConfig().enabled) {
            menu_->addAction(QStringLiteral("OCR 识别"),
                             this, &ScreenshotWidget::recognizeSelection);
        }
        
        {
            auto *s = windowManager_->setting();
            if (!s->gitHubImageBedConfig().token.isEmpty()) {
                menu_->addAction(QStringLiteral("上传图床"), this, [this]() {
                    QPixmap pixmap = originScreen_->copy(currentRect_);
                    if (drawPanel_) {
                        drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
                    }
                    emit actionController_->onUploadRequested(pixmap);
                });
            }
        }

        menu_->addSeparator();
        menu_->addAction(QStringLiteral("退出"), this, &ScreenshotWidget::sigClose);
    }
}

void ScreenshotWidget::recognizeSelection()
{
    if (!drawPanel_) return;

    const PaddleOcrConfig config = windowManager_->setting()->paddleOcrConfig();
    if (!config.enabled) {
        drawPanel_->showToolMessage(QStringLiteral("请先在设置 / 图片中启用 OCR"), true);
        return;
    }

    QPixmap pixmap = originScreen_->copy(currentRect_);
    drawPanel_->drawer()->drawPixmap(pixmap, currentRect_.topLeft());
    if (pixmap.isNull()) {
        drawPanel_->showToolMessage(QStringLiteral("OCR 失败：截图数据为空"), true);
        return;
    }

    drawPanel_->showToolMessage(QStringLiteral("OCR 识别中…"), false);
    auto* client = new PaddleOcrClient(this);
    connect(client, &PaddleOcrClient::succeeded, this, [this, client](const QString& text) {
        QApplication::clipboard()->setText(text);
        if (drawPanel_) {
            drawPanel_->showToolMessage(
                QStringLiteral("OCR 识别成功，文本已复制到剪切板"), false);
        }
        client->deleteLater();
    });
    connect(client, &PaddleOcrClient::failed, this, [this, client](const QString& error) {
        if (drawPanel_) {
            drawPanel_->showToolMessage(QStringLiteral("OCR 失败：%1").arg(error), true);
        }
        client->deleteLater();
    });
    client->recognize(Util::pixmap2ByteArray(pixmap), config);
}

void ScreenshotWidget::moveDrawPanel()
{
    if (drawPanel_) {
        drawPanel_->onReferRectChanged(currentRect_);
    }
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
        if (currentState_ == ScreenState::Exploring) {
		    // 获得截图器当前起始位置
		    startPoint_ = e->pos();
            // 如果用户点击了高亮的窗口，直接进入选择完毕的状态，不再进入自由拖拽框选模式
            if (!getEsthesiaRect().isEmpty() && getEsthesiaRect().contains(startPoint_)) {
                // 不做处理，留给 release 时确认
                isPressed_ = true;
            } else {
		        currentState_ = ScreenState::Selecting;
                currentRect_ = QRect(e->pos(), e->pos());
                initCursor(); // 保持默认选区光标
                isPressed_ = true;
                originPoint_ = e->pos();
                
                QRect oldEsthesia = getEsthesiaRect();
                setEsthesiaRect(QRect()); // 清除高亮框
                if (!oldEsthesia.isEmpty()) {
                    update(oldEsthesia.adjusted(-4, -4, 4, 4));
                }
            }
        } else if (currentState_ == ScreenState::Selecting) {
            isPressed_ = true;
            originPoint_ = e->pos();
        } else if (currentState_ == ScreenState::Editing) {
            isPressed_ = true;
            if (direction_ != DIR_NONE) {
                // Resize action
                onScreenBorderPressed(e->x(), e->y());
            }
            movePos_ = e->pos() - currentRect_.topLeft();
        }
	}
	QWidget::mousePressEvent(e);
}

void ScreenshotWidget::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::RightButton) {
		if (currentState_ == ScreenState::Editing) {
            // 在不点击画笔或者绘制范围内的情况下，可以发出取消信号，但这里为了让 contextMenuEvent 正确执行，不发送取消信号
            // 因为 QWidget 默认右键点击会先触发 mousePressEvent，再触发 contextMenuEvent，最后触发 mouseReleaseEvent
		} else {
			emit sigClose();
		}
		return;
	}
	
    if (e->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(e);
        return;
    }

    if (currentState_ == ScreenState::Exploring) {
        if (isPressed_ && startPoint_ == e->pos() && !getEsthesiaRect().isEmpty()) {
            // 点击了感知窗口
            currentRect_ = getEsthesiaRect();
            setEsthesiaRect(QRect()); // 清除高亮框
            
            // 重新计算八个锚点并通知改变
            onSelectedScreenSizeChanged(currentRect_.width(), currentRect_.height());
            
            // 进入编辑状态
            currentState_ = ScreenState::Editing;
            
            // 弹出绘图面板
            if (drawPanel_) {
                moveDrawPanel();
                drawPanel_->show();
                drawPanel_->raise();
            }
            
            // 隐藏放大器
            amplifierTool_->hide();

            // 自动贴图
            if (windowManager_->setting()->autoPin()) {
                emit windowManager_->sigPin();
            }
            isPressed_ = false;
        }
    } else if (currentState_ == ScreenState::Selecting) {
        // 重新计算八个锚点并通知改变
        onSelectedScreenSizeChanged(currentRect_.width(), currentRect_.height());
        
        // 进入编辑状态
        currentState_ = ScreenState::Editing;
        
        // 弹出绘图面板
        if (drawPanel_) {
            moveDrawPanel();
            drawPanel_->show();
            drawPanel_->raise();
        }
        
        // 隐藏放大器
        amplifierTool_->hide();

        // 自动贴图
        if (windowManager_->setting()->autoPin()) {
            emit windowManager_->sigPin();
        }
        isPressed_ = false;
    } else if (currentState_ == ScreenState::Editing) {
        isPressed_ = false;
        onScreenBorderReleased(e->globalX(), e->globalY());
    }

	QWidget::mouseReleaseEvent(e);
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *e) {
	emit sigCursorPosChanged(e->x(), e->y());
    switch (currentState_) {
        case ScreenState::Exploring:
            if (isPressed_) {
                if ((e->pos() - startPoint_).manhattanLength() > QApplication::startDragDistance()) {
                    // 转为自由框选模式
                    currentState_ = ScreenState::Selecting;
                    currentRect_ = QRect(startPoint_, e->pos()).normalized();
                    initCursor(); // 恢复为彩色小十字或其他初始光标
                    originPoint_ = startPoint_;
                    
                    QRect oldEsthesia = getEsthesiaRect();
                    setEsthesiaRect(QRect()); // 清除高亮框
                    if (!oldEsthesia.isEmpty()) {
                        update(oldEsthesia.adjusted(-4, -4, 4, 4));
                    }
                    
                    onCursorPosChanged(e->x(), e->y());
                }
            } else {
                updateMouse();
                updateCursorDir(e->pos());
            }
            break;
        case ScreenState::Selecting:
            if (isPressed_) {
                amplifierTool_->raise();
                currentRect_ = QRect(originPoint_, e->pos()).normalized();
                onCursorPosChanged(e->x(), e->y());
            } else {
                initCursor();
            }
            break;
        case ScreenState::Editing:
            if(!isPressed_) {
                updateCursorDir(e->pos());
                // 根据方位判断拖拉对应支点
                switch(direction_) {
                case DIR_NONE:
                case DIR_RIGHT:
                case DIR_RIGHT_BOTTOM:
                    originPoint_ = currentRect_.topLeft();
                    break;
                case DIR_RIGHT_TOP:
                    originPoint_ = currentRect_.bottomLeft();
                    break;
                case DIR_LEFT:
                case DIR_LEFT_BOTTOM:
                    originPoint_ = currentRect_.topRight();
                    break;
                case DIR_LEFT_TOP:
                case DIR_TOP:
                    originPoint_ = currentRect_.bottomRight();
                    break;
                case DIR_BOTTOM:
                    originPoint_ = currentRect_.topLeft();
                    break;
                }
            }
            else {
                if (direction_ != DIR_NONE) {
                    // 鼠标在边框上拖动
                    switch(direction_) {
                    case DIR_LEFT:
                        onCursorPosChanged(e->x(), currentRect_.bottomLeft().y() + 1);
                        break;
                    case DIR_RIGHT:
                        onCursorPosChanged(e->x(), currentRect_.bottomRight().y() + 1);
                        break;
                    case DIR_TOP:
                        onCursorPosChanged(currentRect_.topLeft().x(), e->y());
                        break;
                    case DIR_BOTTOM:
                        onCursorPosChanged(currentRect_.bottomRight().x() + 1, e->y());
                        break;
                    case DIR_LEFT_TOP:
                    case DIR_RIGHT_TOP:
                    case DIR_LEFT_BOTTOM:
                    case DIR_RIGHT_BOTTOM:
                        onCursorPosChanged(e->x(), e->y());
                        break;
                    default:
                        break;
                    }
                }
                else {
                    // 鼠标按下移动选区
                    if ((e->pos() - movePos_).manhattanLength() > QApplication::startDragDistance()) {
                        QPoint newPos = e->pos() - movePos_;
                        currentRect_.moveTo(adjustPos(newPos));
                        movePos_ = e->pos() - currentRect_.topLeft();
                        onSelectedScreenSizeChanged(currentRect_.width(), currentRect_.height());
                        onSelectedScreenPosChanged(currentRect_.x(), currentRect_.y());
                        update();
                    }
                }
            }
            break;
    }
	QWidget::mouseMoveEvent(e);
}

void ScreenshotWidget::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	
    if (currentState_ == ScreenState::Exploring) {
	    /// 画全屏背景图
	    const std::shared_ptr<QPixmap> &backgroundPixmap = getBackgroundScreen();
	    painter.drawPixmap(0, 0, *backgroundPixmap);

        if (!getEsthesiaRect().isEmpty()) {
            /// 绘制选区
            QPen pen = painter.pen();
            pen.setColor(Style::AUTO_SELECTED_BORDER_COLOR);
            pen.setWidth(Style::AUTO_SELECTED_BORDER_WIDTH);
            painter.setPen(pen);
            int padding = Style::AUTO_SELECTED_BORDER_WIDTH / 2;
            QRect borderRect = getEsthesiaRect() - QMargins(padding, padding, padding, padding);
            painter.drawRect(borderRect);
            QRect imageRect = borderRect - QMargins(padding, padding, padding, padding);
            painter.drawPixmap(QPoint(imageRect.x(), imageRect.y()), *getGlobalScreen(), imageRect);
        }
    } else if (currentState_ == ScreenState::Selecting || currentState_ == ScreenState::Editing) {
        // 全屏暗色背景图
	    const std::shared_ptr<QPixmap> &backgroundPixmap = getBackgroundScreen();
	    painter.drawPixmap(0, 0, *backgroundPixmap);

        if (!currentRect_.isEmpty()) {
            int bw = Style::DRAW_SELECTED_BORDER_WIDTH;
            
            // 将选区内的原图画出来，这样就能抠出高亮区域
            if (currentRect_.width() > 0 && currentRect_.height() > 0) {
                painter.drawPixmap(QPoint(currentRect_.x(), currentRect_.y()), *getGlobalScreen(), currentRect_);
            }
            
            /// 绘制边框线
            QPen pen(Style::DRAW_SELECTED_BORDER_COLOR, bw);
            painter.setPen(pen);
            painter.drawRect(currentRect_ - QMargins(bw/2, bw/2, bw/2, bw/2));
            
            if (currentState_ == ScreenState::Editing) {
                // 绘制边框上八个点
                QBrush brush(Qt::red);
                for (int i = 0; i < listMarker_.size(); i++) {
                    QRect rect(listMarker_[i].x(), listMarker_[i].y(), MARKERT_WIDTH, MARKERT_WIDTH);
                    painter.fillRect(rect, brush);
                }

                if (drawPanel_) {
                    painter.setClipRect(currentRect_);
                    drawPanel_->drawer()->onPaint(painter);
                    painter.setClipping(false);
                }
            }
        }
    }
}

void ScreenshotWidget::onCursorPosChanged(int x, int y) 
{
    const QRect oldRect = currentRect_;

	if (x < 0 || y < 0) {
		return;
	}

	int rx = (x >= originPoint_.x()) ? originPoint_.x() : x;
	int ry = (y >= originPoint_.y()) ? originPoint_.y() : y;
	int rw = abs(x - originPoint_.x());
	int rh = abs(y - originPoint_.y());

	// 改变大小
    const QRect newRect(rx, ry, rw, rh);
    if (newRect == oldRect) {
        return;
    }
	currentRect_ = newRect;
    
    // 采用全屏刷新，杜绝因 dirty 计算不准确导致的残影和黑块问题
    update();

    if (currentState_ == ScreenState::Editing || currentState_ == ScreenState::Selecting) {
        onSelectedScreenSizeChanged(currentRect_.width(), currentRect_.height());
        onSelectedScreenPosChanged(currentRect_.x(), currentRect_.y());
    }
}

void ScreenshotWidget::updateCursorDir(const QPoint &cursor) {
    if (currentState_ == ScreenState::Exploring) {
        initCursor();
        return;
    } else if (currentState_ == ScreenState::Selecting) {
        initCursor();
        return;
    }

	QPoint tl = currentRect_.topLeft();
	QPoint br = currentRect_.bottomRight();
	int x = cursor.x();
	int y = cursor.y();
	int bw = BORDER_ESTHESIA_WIDTH;

    auto inRange = [](int val, int min, int max) { return val >= min && val <= max; };

    bool l = inRange(x, tl.x(), tl.x() + bw);
    bool r = inRange(x, br.x() - bw, br.x());
    bool t = inRange(y, tl.y(), tl.y() + bw);
    bool b = inRange(y, br.y() - bw, br.y());
    bool x_in = inRange(x, tl.x(), br.x());
    bool y_in = inRange(y, tl.y(), br.y());

    direction_ = DIR_NONE;
    Qt::CursorShape shape = Qt::ArrowCursor;

    // Simplify direction logic using a map or structural checks
    if (l && t) { direction_ = DIR_LEFT_TOP; shape = Qt::SizeFDiagCursor; }
    else if (r && b) { direction_ = DIR_RIGHT_BOTTOM; shape = Qt::SizeFDiagCursor; }
    else if (l && b) { direction_ = DIR_LEFT_BOTTOM; shape = Qt::SizeBDiagCursor; }
    else if (r && t) { direction_ = DIR_RIGHT_TOP; shape = Qt::SizeBDiagCursor; }
    else if (l && y_in) { direction_ = DIR_LEFT; shape = Qt::SizeHorCursor; }
    else if (r && y_in) { direction_ = DIR_RIGHT; shape = Qt::SizeHorCursor; }
    else if (t && x_in) { direction_ = DIR_TOP; shape = Qt::SizeVerCursor; }
    else if (b && x_in) { direction_ = DIR_BOTTOM; shape = Qt::SizeVerCursor; }
    else if (currentState_ == ScreenState::Editing && currentRect_.contains(cursor)) { shape = Qt::SizeAllCursor; }

    if (direction_ != DIR_NONE) {
        this->setCursor(QCursor(shape));
    } else if (shape == Qt::SizeAllCursor) {
        if (drawPanel_ && drawPanel_->drawer() && drawPanel_->drawer()->isDraw()) {
            this->setCursor(drawPanel_->drawer()->mode().cursor());
        } else {
            this->setCursor(QCursor(shape));
        }
    } else {
        if (currentState_ == ScreenState::Editing && drawPanel_ && drawPanel_->drawer() && drawPanel_->drawer()->isDraw()) {
            this->setCursor(drawPanel_->drawer()->mode().cursor());
        } else {
            initCursor();
        }
    }
    
    if (drawPanel_ && drawPanel_->drawer()) {
	    drawPanel_->drawer()->setEnable(direction_ == DIR_NONE);
    }
}

QPoint ScreenshotWidget::adjustPos(QPoint p)
{
    const QRect bounds = Util::desktopLocalRect();
	if (p.x() < 0) {
		p.setX(0);
	} else if (p.x() + currentRect_.width() > bounds.width()) {
		p.setX(bounds.width() - currentRect_.width());
	}
	if (p.y() < 0) {
		p.setY(0);
	} else if (p.y() + currentRect_.height() > bounds.height()) {
		p.setY(bounds.height() - currentRect_.height());
	}
	return p;
}

QRect ScreenshotWidget::adjustRect(QRect r)
{
    return Util::clampToDesktopLocal(r);
}

void ScreenshotWidget::onSelectRectChanged(int left, int top, int right, int bottom)
{
    const QRect oldRect = currentRect_;

	QRect newRect;
	newRect.setLeft(currentRect_.left() + left);
	newRect.setRight(currentRect_.right() + right);
	newRect.setTop(currentRect_.top() + top);
	newRect.setBottom(currentRect_.bottom() + bottom);
	if (newRect.width() <= 0 || newRect.height() <= 0) {
		return;
	}

	if (adjustRect(newRect) == newRect) {
		currentRect_ = newRect;
		// 采用全屏刷新，杜绝因 dirty 计算不准确导致的残影和黑块问题
        update();

        if (currentState_ == ScreenState::Editing) {
            onSelectedScreenSizeChanged(currentRect_.width(), currentRect_.height());
            onSelectedScreenPosChanged(currentRect_.x(), currentRect_.y());
        }
	}
}

void ScreenshotWidget::scaledRect(int direction)
{
	if (!originScreen_) {
		return;
	}

	QImage image = originScreen_->toImage();
    const QRect& r = currentRect_;

	auto isColorSimilar = [&image](QRgb rgb, const QPoint &from, const QPoint &to) -> bool {
        QColor src(rgb);
		for (int i = from.x(); i <= to.x(); i++) {
			for (int j = from.y(); j <= to.y(); j++) {
				if (image.rect().contains(i, j)) {
                    if (Util::colorDistance(src, image.pixel(i, j)) > 50) {
                        return false;
                    }
				}
			}
		}
        return true;
	};

    auto scan = [&](const QPoint& seed, int maxLimit, int dirFactor, 
                    std::function<void(int, QPoint&, QPoint&)> getPoints) -> int {
        if (!image.rect().contains(seed)) return 0;
        QRgb clr = image.pixel(seed);
        if (clr == 0) return 0;

        int bestOffset = 0;
        for (int i = 1; i <= maxLimit; i++) {
            int offset = i * dirFactor;
            QPoint p1, p2;
            getPoints(offset, p1, p2);
            if (isColorSimilar(clr, p1, p2)) {
                bestOffset = offset;
            } else {
                break;
            }
        }
        return bestOffset;
    };

    int offsetLeft = scan(QPoint(r.left(), r.center().y()), image.width(), direction, 
        [&](int off, QPoint& p1, QPoint& p2) { p1 = {r.left() + off, r.top()}; p2 = {r.left() + off, r.bottom()}; });

    int offsetRight = scan(QPoint(r.right(), r.center().y()), image.width(), -direction, 
        [&](int off, QPoint& p1, QPoint& p2) { p1 = {r.right() + off, r.top()}; p2 = {r.right() + off, r.bottom()}; });

    int offsetTop = scan(QPoint(r.center().x(), r.top()), image.height(), direction, 
        [&](int off, QPoint& p1, QPoint& p2) { p1 = {r.left(), r.top() + off}; p2 = {r.right(), r.top() + off}; });

    int offsetBottom = scan(QPoint(r.center().x(), r.bottom()), image.height(), -direction, 
        [&](int off, QPoint& p1, QPoint& p2) { p1 = {r.left(), r.bottom() + off}; p2 = {r.right(), r.bottom() + off}; });

	onSelectRectChanged(offsetLeft, offsetTop, offsetRight, offsetBottom);
}

void ScreenshotWidget::updateMouse(bool force) {
    if (!force) {
        if (mouseUpdateElapsed_.isValid()) {
            const qint64 elapsed = mouseUpdateElapsed_.elapsed();
            if (elapsed < MOUSE_UPDATE_INTERVAL_MS) {
                if (mouseUpdateTimer_ && !mouseUpdateTimer_->isActive()) {
                    mouseUpdateTimer_->start(MOUSE_UPDATE_INTERVAL_MS - static_cast<int>(elapsed));
                }
                return;
            }
        }
    } else if (mouseUpdateTimer_ && mouseUpdateTimer_->isActive()) {
        mouseUpdateTimer_->stop();
    }

    mouseUpdateElapsed_.restart();

    IRectDetector* detector = activeDetector();
    if (!detector) {
        return;
    }

    QRect targetRect = detector->detectRect(cursor().pos(), this);
    
    // 如果探测器没有返回有效的区域，降级为全桌面
    if (targetRect.isEmpty()) {
        targetRect = Util::desktopLocalRect();
    }
    
    if (getEsthesiaRect() != targetRect) {
        setEsthesiaRect(targetRect);
        update();
    }
}

void ScreenshotWidget::handleArrowKeyPress(QKeyEvent *e)
{
    int left = 0, top = 0, right = 0, bottom = 0;
    
    // Calculate movement based on key and modifiers
    auto calcMove = [&](int& primary, int& secondary, int val) {
        if (e->modifiers() & Qt::ControlModifier) {
            primary = val;
            secondary = 0;
        } else if (e->modifiers() & Qt::ShiftModifier) {
            primary = 0;
            secondary = val;
        } else {
            primary = val;
            secondary = val;
        }
    };

    switch(e->key()) {
        case Qt::Key_Left:  calcMove(left, right, -1); break;
        case Qt::Key_Right: calcMove(left, right, 1);  break;
        case Qt::Key_Up:    calcMove(top, bottom, -1); break;
        case Qt::Key_Down:  calcMove(top, bottom, 1);  break;
        default: return;
    }

	if (currentState_ == ScreenState::Editing) {
		onSelectRectChanged(left, top, right, bottom);
		moveDrawPanel();
	} else {
		QPoint p(0, 0);
		if (left != 0) {
			p.setX(left);
		} else if (right != 0) {
			p.setX(right);
		} else if (top != 0) {
			p.setY(top);
		} else if (bottom != 0) {
			p.setY(bottom);
		}
		if (!p.isNull()) {
			QCursor cursor;
			cursor.setPos(cursor.pos() + p);
		}
	}
}
	
void ScreenshotWidget::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Escape) {
		emit sigClose();
		return;
	}
    
    if (e->key() == Qt::Key_Equal) {
		if (currentState_ == ScreenState::Editing) {
			scaledRect(-1);
			moveDrawPanel();
		}
        return;
	} 
    
    if (e->key() == Qt::Key_Minus) {
		if (currentState_ == ScreenState::Editing) {		
			scaledRect(1);
			moveDrawPanel();
		}
        return;
	}
    
    if (e->key() == Qt::Key_C) {
		if (amplifierTool_ && amplifierTool_->isVisible()) {
			QClipboard *clipboard = QApplication::clipboard();
			clipboard->setText(amplifierTool_->getCursorPointColor());
            return;
		}
	}

    if (e->key() == Qt::Key_Alt) {
        useOpenCVMode_ = !useOpenCVMode_;

        // 记住模式
        if (windowManager_ && windowManager_->setting()) {
            windowManager_->setting()->setOpenCVMode(useOpenCVMode_);
        }

        if (amplifierTool_) {
            amplifierTool_->setOpenCVMode(useOpenCVMode_);
        }

        // 统一调用初始化，屏蔽掉具体的类型判断，利用多态处理
        activeDetector()->init(originScreen_, [this]() {
            this->updateMouse(true);
        });

        // 立即清除当前的高亮框，强制下一帧重新计算
        setEsthesiaRect(QRect());

        updateMouse(true);
        return;
    }

    if (currentState_ == ScreenState::Editing && menu_) {
        QString keyStr = Util::strKeyEvent(e);
        if (!keyStr.isEmpty()) {
            QList<QAction*> actions = menu_->actions();
            for (int i = 0; i < actions.size(); i++) {
                QKeySequence seq = actions[i]->shortcut();
                if (!seq.isEmpty() && seq == keyStr) {
                    emit actions[i]->triggered();
                    e->ignore();
                    return;
                }
            }
        }
    }

    handleArrowKeyPress(e);
	e->ignore();
}

void ScreenshotWidget::onLongScreenshotRequested(const QRect &captureRect)
{
	windowManager_->openLongScreenshotWidget(captureRect, originScreen_);
	emit sigClose();
}

void ScreenshotWidget::onGifRecordingRequested(const QRect &captureRect)
{
    windowManager_->openGifRecorderWidget(captureRect);
    emit sigClose();
}
