#include "DrawPanel.h"

#include <QPen>
#include <QPainter>
#include <QPoint>
#include <QPolygon>
#include <QStaticText>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QDebug>
#include <QPushButton>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QApplication>
#include <QPaintEvent>
#include <QTimer>
#include "core/theme/AppTheme.h"
#include "core/theme/ThemeManager.h"
#include <algorithm>
#include <QMenu>
#include <QVBoxLayout>
#include "core/theme/ThemeIcon.h"
#include "core/theme/UiStyler.h"
#include <QClipboard>
#include "core/settings/SettingModel.h"
#include "modules/capture/pin/PinWidget.h"
#include "modules/capture/screenshot/Screenshot.h"
#include "shared/ui/TextEdit.h"
#include "core/foundation/Constants.h"
#include "core/platform/Util.h"

#include <opencv2/opencv.hpp>

namespace {
int mosaicBlockWidth()
{
    return Util::scaleSize(DrawSettings::fontSize() * 1.5);
}

QCursor createMosaicCursor()
{
    const int size = std::min(mosaicBlockWidth(), 30);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(200, 200, 200, 100));
    painter.setPen(QPen(QColor(100, 100, 100, 200), 1));
    painter.drawEllipse(0, 0, size - 1, size - 1);
    return QCursor(pixmap, size / 2, size / 2);
}

QCursor createPolyLineCursor()
{
    const int size = std::max(DrawSettings::penWidth(), Util::scaleSize(6));
    const int canvasSize = size + Util::scaleSize(4);

    QPixmap pixmap(canvasSize, canvasSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(DrawSettings::currentColor());
    painter.setPen(QPen(QColor(0, 0, 0, 100), Util::scaleSize(1) > 0 ? Util::scaleSize(1) : 1));
    painter.drawEllipse(Util::scaleSize(2), Util::scaleSize(2), size - 1, size - 1);
    painter.setPen(QPen(Qt::white, Util::scaleSize(1) > 0 ? Util::scaleSize(1) : 1));
    painter.drawEllipse(Util::scaleSize(2), Util::scaleSize(2), size - 1, size - 1);
    return QCursor(pixmap, canvasSize / 2, canvasSize / 2);
}

class ImageToolsButton : public QPushButton
{
public:
    explicit ImageToolsButton(const QIcon& icon, QWidget* parent)
        : QPushButton(icon, QString(), parent) {}

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPushButton::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::tokens().textSecondary);
        const qreal margin = Util::scaleSize(4);
        const qreal arrowWidth = Util::scaleSize(6);
        const qreal right = width() - margin;
        const qreal bottom = height() - margin;
        QPolygonF triangle;
        triangle << QPointF(right - arrowWidth, bottom - arrowWidth / 2)
                 << QPointF(right, bottom - arrowWidth / 2)
                 << QPointF(right - arrowWidth / 2, bottom);
        painter.drawPolygon(triangle);
    }
};
}

DrawPanel::DrawPanel(SettingModel* settings, QWidget *parent, QWidget *drawWidget)
    : QWidget(parent), settings_(settings), drawer_(drawWidget)
{
    this->setCursor(Util::multicolorCursor());
    setupButtonGroup();
    setupPanels();
    setupButtonsAndLayout(parent != nullptr);
    this->setFixedHeight(Util::scaleSize(40));
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void DrawPanel::setupButtonGroup()
{
    this->setObjectName("DrawPanel");
    shapeGroup_ = new QButtonGroup(this);
    shapeGroup_->setExclusive(false);
    connect(shapeGroup_, &QButtonGroup::buttonClicked, this, &DrawPanel::onShapeBtnClicked);
}

void DrawPanel::setupPanels()
{
    // 使用通用DPI适配方法调整按钮大小
    const int btnSize = Util::scaleSize(32);
    const int iconSize = Util::scaleSize(18);

    pbFont_ = new QPushButton(ThemeIcon::icon("color.png", IconTone::Default, iconSize), "", this);
    pbFont_->setFixedSize(btnSize, btnSize);
    pbFont_->setIconSize(QSize(iconSize, iconSize));
    pbFont_->setToolTip(QStringLiteral("颜色与线条"));
    UiStyler::setRole(pbFont_, UiRole::IconButton);
    connect(pbFont_, &QPushButton::clicked, this, &DrawPanel::onColorBtnClicked);

    drawSettings_ = new DrawSettings(this);
    drawSettings_->setVisible(false);
    connect(drawSettings_, &DrawSettings::sigChanged, this, &DrawPanel::onSettingChanged);

    toolMessageLabel_ = new QLabel(this);
    toolMessageLabel_->setObjectName(QStringLiteral("DrawPanelToolMessage"));
    toolMessageLabel_->setVisible(false);
    toolMessageLabel_->setWordWrap(true);
    toolMessageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    toolMessageLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const int pad = Util::scaleSize(6);
    toolMessageLabel_->setContentsMargins(pad, pad / 2, pad, pad / 2);

    toolMessageTimer_ = new QTimer(this);
    toolMessageTimer_->setSingleShot(true);
    connect(toolMessageTimer_, &QTimer::timeout, this, &DrawPanel::clearToolMessage);
}

void DrawPanel::setupButtonsAndLayout(bool hasParent)
{
    // 使用通用DPI适配方法计算尺寸
    const int btnSize = Util::scaleSize(32);
    const int iconSize = Util::scaleSize(18);

    auto createSeparator = [this, btnSize]() -> QFrame* {
        auto* separator = new QFrame(this);
        separator->setObjectName(QStringLiteral("DrawPanelSeparator"));
        separator->setFixedSize(Util::scaleSize(1), qMax(1, btnSize - Util::scaleSize(12)));
        return separator;
    };

    auto createShapeBtn = [this, btnSize, iconSize](const QString &iconName, const QString &tooltip) -> QPushButton* {
        QPushButton* pb = new QPushButton(
            ThemeIcon::icon(iconName, IconTone::Default, iconSize), "", this);
        pb->setToolTip(tooltip);
        pb->setCheckable(true);
        pb->setFixedSize(btnSize, btnSize);
        pb->setIconSize(QSize(iconSize, iconSize));
        UiStyler::setRole(pb, UiRole::IconButton);
        shapeGroup_->addButton(pb);
        return pb;
    };

    auto createActionBtn = [this, btnSize, iconSize](const QString &iconName, const QString &tooltip) -> QPushButton* {
        QPushButton* pb = new QPushButton(
            ThemeIcon::icon(iconName, IconTone::Default, iconSize), "", this);
        pb->setToolTip(tooltip);
        pb->setFixedSize(btnSize, btnSize);
        pb->setIconSize(QSize(iconSize, iconSize));
        UiStyler::setRole(pb, UiRole::IconButton);
        return pb;
    };

    QPushButton* pbPolyLine = createShapeBtn("polyline.png", QStringLiteral("折线"));
    QPushButton* pbLine = createShapeBtn("line.png", QStringLiteral("直线"));
    QPushButton* pbArrow = createShapeBtn("arrow.png", QStringLiteral("箭头"));
    QPushButton* pbRectangle = createShapeBtn("rectangle.png", QStringLiteral("矩形"));
    QPushButton* pbEllipse = createShapeBtn("ellipse.png", QStringLiteral("椭圆"));
    QPushButton* pbText = createShapeBtn("text.png", QStringLiteral("文本（Ctrl+Enter 完成，Esc 取消）"));

    QPushButton* pbMosaic = createShapeBtn("mosaic.png", QStringLiteral("马赛克"));
    const int imageToolsIconSize = Util::scaleSize(16);
    QPushButton* pbImageTools = new ImageToolsButton(
        ThemeIcon::icon("tools-solid.png", IconTone::Default, imageToolsIconSize), this);
    pbImageTools->setToolTip(QStringLiteral("更多图像工具"));
    pbImageTools->setFixedSize(btnSize, btnSize);
    pbImageTools->setIconSize(QSize(imageToolsIconSize, imageToolsIconSize));
    pbImageTools->setProperty("iconBaseSize", 16);
    UiStyler::setRole(pbImageTools, UiRole::IconButton);
    QMenu* mosaicMenu = new QMenu(pbImageTools);
    mosaicMenu->addAction(QStringLiteral("识别二维码/条码"), this, &DrawPanel::sigScanCode);
    QMenu* enhanceMenu = mosaicMenu->addMenu(QStringLiteral("图像增强"));
    enhanceMenu->addAction(QStringLiteral("自动增强"), this, [this]() { emit sigEnhance(0); });
    enhanceMenu->addAction(QStringLiteral("提亮"), this, [this]() { emit sigEnhance(1); });
    enhanceMenu->addAction(QStringLiteral("对比度"), this, [this]() { emit sigEnhance(2); });
    enhanceMenu->addAction(QStringLiteral("锐化"), this, [this]() { emit sigEnhance(3); });
    enhanceMenu->addAction(QStringLiteral("降噪"), this, [this]() { emit sigEnhance(4); });
    mosaicMenu->addAction(QStringLiteral("智能打码"), this, &DrawPanel::sigSmartMask);
    mosaicMenu->addAction(QStringLiteral("自动裁边"), this, &DrawPanel::sigAutoCrop);
    mosaicMenu->addAction(QStringLiteral("提取主色"), this, &DrawPanel::sigExtractColors);

    connect(pbImageTools, &QPushButton::clicked, this, [pbImageTools, mosaicMenu]() {
        mosaicMenu->exec(pbImageTools->mapToGlobal(QPoint(0, pbImageTools->height())));
    });

    QPushButton* pbAskAi = createActionBtn("llm.png", QStringLiteral("问 AI"));
    QPushButton* pbUndo = createActionBtn("undo.png", QStringLiteral("撤销"));
    QPushButton* pbSticker = createActionBtn("pin.png", QStringLiteral("贴图"));
    QPushButton* pbLongScreenshot = createActionBtn("long_screenshot.png", QStringLiteral("长截图"));
    QPushButton* pbGifRecording = new QPushButton(QStringLiteral("GIF"), this);
    pbGifRecording->setToolTip(QStringLiteral("录制 GIF"));
    pbGifRecording->setFixedSize(btnSize, btnSize);
    QFont gifFont = pbGifRecording->font();
    gifFont.setPixelSize(qMax(7, Util::scaleSize(8)));
    gifFont.setBold(true);
    pbGifRecording->setFont(gifFont);
    QPushButton* pbOcr = new QPushButton(QStringLiteral("OCR"), this);
    pbOcr->setToolTip(QStringLiteral("识别文字并复制到剪切板"));
    pbOcr->setFixedSize(btnSize, btnSize);
    QFont ocrFont = pbOcr->font();
    ocrFont.setPixelSize(qMax(7, Util::scaleSize(8)));
    ocrFont.setBold(true);
    pbOcr->setFont(ocrFont);
    pbOcr->setVisible(settings_ && settings_->paddleOcrConfig().enabled);
    QPushButton* pbSave = createActionBtn("save.png", QStringLiteral("保存"));
    QPushButton* pbFinished = createActionBtn("clipboard.png", QStringLiteral("剪切板"));
    pbFinished->setIcon(ThemeIcon::icon("clipboard.png", IconTone::OnAccent, iconSize));
    UiStyler::setRole(pbFinished, UiRole::PrimaryButton);

    connect(pbAskAi, &QPushButton::clicked, this, &DrawPanel::sigAskAi);
    connect(pbUndo, &QPushButton::clicked, &drawer_, &Drawer::undo);
    connect(pbSticker, &QPushButton::clicked, this, &DrawPanel::sigSticker);
    connect(pbLongScreenshot, &QPushButton::clicked, this, &DrawPanel::sigLongScreenshot);
    connect(pbGifRecording, &QPushButton::clicked, this, &DrawPanel::sigGifRecording);
    connect(pbOcr, &QPushButton::clicked, this, &DrawPanel::sigOcr);
    connect(pbSave, &QPushButton::clicked, this, &DrawPanel::sigSave);
    connect(pbFinished, &QPushButton::clicked, this, &DrawPanel::sigFinished);

    DrawMode polylineMode(DrawMode::PolyLine);
    DrawMode lineMode(DrawMode::Line);
    DrawMode arrowMode(DrawMode::Arrow);
    arrowMode.pen().setStyle(Qt::NoPen);
    arrowMode.brush().setStyle(Qt::SolidPattern);
    DrawMode rectangleMode(DrawMode::Rectangle);
    DrawMode ellipseMode(DrawMode::Ellipse);
    DrawMode textMode(DrawMode::Text);
    DrawMode mosaicMode(DrawMode::Mosaic);
    btns_.push_back(qMakePair(pbPolyLine, polylineMode));
    btns_.push_back(qMakePair(pbLine, lineMode));
    btns_.push_back(qMakePair(pbArrow, arrowMode));
    btns_.push_back(qMakePair(pbRectangle, rectangleMode));
    btns_.push_back(qMakePair(pbEllipse, ellipseMode));
    btns_.push_back(qMakePair(pbText, textMode));
    btns_.push_back(qMakePair(pbMosaic, mosaicMode));

    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(Util::scaleSize(4), Util::scaleSize(4),
                                Util::scaleSize(4), Util::scaleSize(4));
    hLayout->setSpacing(Util::scaleSize(2));
    hLayout->addWidget(pbFont_);
    hLayout->addWidget(createSeparator(), 0, Qt::AlignVCenter);
    for (int i = 0; i < 6; ++i) hLayout->addWidget(btns_.at(i).first);
    hLayout->addWidget(pbUndo);
    hLayout->addWidget(createSeparator(), 0, Qt::AlignVCenter);
    hLayout->addWidget(pbMosaic);
    hLayout->addWidget(pbImageTools);
    hLayout->addWidget(pbAskAi);
    hLayout->addWidget(pbOcr);
    hLayout->addWidget(createSeparator(), 0, Qt::AlignVCenter);
    hLayout->addWidget(pbSticker);
    hLayout->addWidget(pbLongScreenshot);
    hLayout->addWidget(pbGifRecording);
    hLayout->addWidget(createSeparator(), 0, Qt::AlignVCenter);
    hLayout->addWidget(pbSave);
    hLayout->addWidget(pbFinished);

    QVBoxLayout *mLayout = new QVBoxLayout(this);
    mLayout->setContentsMargins(0, 0, 0, 0);
    mLayout->setSpacing(0);
    mLayout->addLayout(hLayout);
    mLayout->addWidget(drawSettings_);
    mLayout->addWidget(toolMessageLabel_);
    mLayout->addStretch();

    if (!hasParent) {
        setWindowFlags(Qt::ToolTip);
        pbSticker->hide();
        pbLongScreenshot->hide();
        pbGifRecording->hide();
    }
}

DrawMode DrawPanel::getMode()
{
    DrawMode mode;
    for (const auto &buttonModePair : btns_) {
        if (!buttonModePair.second.isNone() && buttonModePair.first->isChecked()) {
            mode = buttonModePair.second;
            break;
        }
    }

    applyCurrentStyle(mode);
    mode.updateCursor();
    return mode;
}

void DrawPanel::applyCurrentStyle(DrawMode &mode) const
{
    mode.pen().setColor(DrawSettings::currentColor());
    mode.pen().setWidth(DrawSettings::penWidth());
    mode.brush().setColor(DrawSettings::currentColor());
    mode.font().setPixelSize(DrawSettings::fontSize());
}

void DrawPanel::refreshPanelHeight()
{
    const int toolbarHeight = Util::scaleSize(40);
    int height = toolbarHeight;
    if (drawSettings_ && drawSettings_->isVisible()) {
        height += drawSettings_->height();
    }
    if (toolMessageLabel_ && toolMessageLabel_->isVisible()) {
        const int minMsgWidth = Util::scaleSize(280);
        const int maxMsgWidth = referRect_.isValid()
            ? qMax(minMsgWidth, qMin(referRect_.width(), Util::scaleSize(420)))
            : Util::scaleSize(360);
        const int msgWidth = qMax(minMsgWidth, qMax(width(), maxMsgWidth));
        toolMessageLabel_->setFixedWidth(msgWidth);
        toolMessageLabel_->adjustSize();
        height += toolMessageLabel_->sizeHint().height() + Util::scaleSize(4);
        if (width() < msgWidth) {
            setMinimumWidth(msgWidth);
        }
    }
    setFixedHeight(height);
}

void DrawPanel::showToolMessage(const QString& message, bool isError)
{
    if (!toolMessageLabel_) {
        return;
    }

    const ThemeTokens& tokens = ThemeManager::tokens();
    const QColor tone = isError ? tokens.danger : tokens.success;
    QColor background = tone;
    background.setAlpha(tokens.theme == AppTheme::Dark ? 48 : 24);

    toolMessageLabel_->setStyleSheet(
        QStringLiteral("QLabel#DrawPanelToolMessage {"
                       " background-color:%1; color:%2;"
                       " border-top:1px solid %3; font-size:%4px; }")
            .arg(background.name(QColor::HexArgb), tone.name(), tokens.border.name())
            .arg(Util::scaleSize(12)));
    toolMessageLabel_->setText(message);
    toolMessageLabel_->setVisible(true);
    toolMessageLabel_->setToolTip(isError ? QString() : QStringLiteral("内容已复制，可选中文本再次复制"));
    refreshPanelHeight();
    adjustPos();

    if (toolMessageTimer_) {
        toolMessageTimer_->start(isError ? 3500 : 8000);
    }
}

void DrawPanel::clearToolMessage()
{
    if (!toolMessageLabel_ || !toolMessageLabel_->isVisible()) {
        return;
    }
    toolMessageLabel_->clear();
    toolMessageLabel_->setVisible(false);
    toolMessageLabel_->setToolTip(QString());
    setMinimumWidth(0);
    refreshPanelHeight();
    adjustPos();
}

void DrawPanel::adjustPos()
{
    static double lastScaleFactor = -1.0;
    double currentScale = Util::getScreenScaleFactor();
    if (qAbs(currentScale - lastScaleFactor) > 0.01) {
        lastScaleFactor = currentScale;
        
        const int btnSize = Util::scaleSize(32);
        const int iconSize = Util::scaleSize(18);
        
        QList<QPushButton*> btns = this->findChildren<QPushButton*>();
        for (QPushButton* btn : btns) {
            if (btn->parent() == this) {
                btn->setFixedSize(btnSize, btnSize);
                if (!btn->icon().isNull()) {
                    const int baseIconSize = btn->property("iconBaseSize").toInt();
                    const int adjustedIconSize = baseIconSize > 0
                        ? Util::scaleSize(baseIconSize) : iconSize;
                    btn->setIconSize(QSize(adjustedIconSize, adjustedIconSize));
                }
            }
        }
        
        if (drawSettings_) {
            drawSettings_->rescaleForDpi();
        }
        refreshPanelHeight();
    } else if (toolMessageLabel_ && toolMessageLabel_->isVisible()) {
        refreshPanelHeight();
    }

    int x = referRect_.x() + referRect_.width() - this->width();
    int y = referRect_.y() + referRect_.height() + 2;
    const QRect screenRect = Util::desktopRect();
    if (x + this->width() > screenRect.width()) {
        x = screenRect.width() - this->width();
    }
    if (y + this->height() > screenRect.height()) {
        y = referRect_.y() - this->height() - 2;
    }
    x = qMax(x, 2);
    y = qMax(y, 2);
    this->move(x, y);
}

Drawer* DrawPanel::drawer()
{
    return &drawer_;
}

void DrawPanel::cancelChecked()
{
    for (const auto &buttonModePair : btns_) {
        if (!buttonModePair.first->isChecked()) {
            continue;
        }
        buttonModePair.first->setChecked(false);
        drawer_.setMode(DrawMode(DrawMode::None));
        break;
    }
}

int DrawPanel::fontSize()
{
    return DrawSettings::fontSize();
}

QColor DrawPanel::currentColor()
{
    return DrawSettings::currentColor();
}

void DrawPanel::onReferRectChanged(const QRect &rect)
{
    referRect_ = rect;
    drawer_.setDrawRect(rect);
    adjustPos();
}

void DrawPanel::onShapeBtnClicked(QAbstractButton* btn)
{
    uncheckOtherButtons(btn);

    if (btn->isChecked()) {
        drawer_.setMode(getMode());
    } else {
        drawer_.setMode(DrawMode(DrawMode::None));
    }
}

void DrawPanel::uncheckOtherButtons(QAbstractButton *checkedButton)
{
    for (const auto &buttonModePair : btns_) {
        if (buttonModePair.first != checkedButton && buttonModePair.first->isChecked()) {
            buttonModePair.first->setChecked(false);
        }
    }
}

void DrawPanel::onColorBtnClicked()
{
    drawSettings_->setVisible(!drawSettings_->isVisible());
    refreshPanelHeight();
    adjustPos();
}

void DrawPanel::onSettingChanged(int fontSize, QColor color)
{
    drawer_.setMode(getMode());
}

void DrawPanel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), ThemeManager::tokens().surfaceRaised);
    QWidget::paintEvent(event);

    // 左右下边缘绘制浅色细边框，避免面板与下方浅色背景/窗口融合
    const ThemeTokens& tokens = ThemeManager::tokens();
    QColor lightBorder = tokens.textSecondary;
    lightBorder.setAlpha(tokens.theme == AppTheme::Dark ? 110 : 90);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(lightBorder, Util::scaleSize(1)));
    const int w = width() - 1;
    const int h = height() - 1;
    painter.drawLine(0, 0, 0, h);      // 左
    painter.drawLine(w, 0, w, h);      // 右
    painter.drawLine(0, h, w, h);      // 下
}

void DrawPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    adjustPos();
}

void DrawPanel::hideEvent(QHideEvent *event)
{
    clearToolMessage();
    QWidget::hideEvent(event);
}

void DrawPanel::mouseReleaseEvent(QMouseEvent*event)
{
    event->accept();
}

//////////////////////////////////////////////////////////////////////////
DrawMode::DrawMode() 
    : shape_(None)
    , cursor_(Qt::ArrowCursor)
{
    init();
}

DrawMode::DrawMode(Shape shape)
    : shape_(shape)
{
    init();
}

void DrawMode::init()
{
    pen_.setColor(Qt::red);
    pen_.setWidth(3);
    brush_.setColor(Qt::red);
    brush_.setStyle(Qt::NoBrush);

    switch (shape_) {
    case None:
        cursor_ = Qt::SizeAllCursor;
        break;
    case Text:
        cursor_ = Qt::IBeamCursor;
        break;
    case Mosaic:
        cursor_ = Qt::ArrowCursor;
        break;
    case Bitmap:
        cursor_ = Qt::CrossCursor;
        break;
    default:
        cursor_ = Qt::CrossCursor;
        break;
    }
}

bool DrawMode::isNone() const
{
    return shape_ == None;
}

bool DrawMode::isValid() const
{
    if (isNone()) {
        return false;
    }

    if (isPathShape()) {
        return !points_.isEmpty();
    }

    if (shape_ == Text) {
        return !text_.isEmpty();
    }

    if (shape_ == Bitmap) {
        return !bitmap_.isNull() && bitmapRect_.isValid();
    }

    // 起止距离太短认为是一个无效的绘制
    return (start_ - end_).manhattanLength() > QApplication::startDragDistance();
}

void DrawMode::setPos(const QPoint &start, const QPoint &end)
{
    start_ = start;
    end_ = end;
}

void DrawMode::addPos(const QPoint &pos)
{
    points_.push_back(pos);
}

bool DrawMode::isPathShape() const
{
    return shape_ == PolyLine || shape_ == Mosaic;
}

void DrawMode::initMosaicBrush(const QImage &baseImage, const QPoint &brushOrigin)
{
    if (baseImage.isNull()) {
        return;
    }

    brushOrigin_ = brushOrigin;

    // 获取需要马赛克的方块大小
    const int blockWidth = mosaicBlockWidth(); // 稍微调小一点，保留更多原始形状

    // 转换为 OpenCV Mat 处理
    QImage formattedSrc = baseImage.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat mat(formattedSrc.height(), formattedSrc.width(), CV_8UC4, (void*)formattedSrc.constBits(), formattedSrc.bytesPerLine());

    cv::Mat small, result;
    // 缩小 (实现马赛克效果)
    cv::resize(mat, small, cv::Size(std::max(1, mat.cols / blockWidth), std::max(1, mat.rows / blockWidth)), 0, 0, cv::INTER_AREA);
    // 放大
    cv::resize(small, result, mat.size(), 0, 0, cv::INTER_NEAREST);

    // 【新增】：高斯模糊叠加，模拟主流产品均匀涂抹的柔和背景感
    // 先对原图进行强力模糊，然后按照一定比例混合，这样既有马赛克的颗粒感，
    // 又有模糊的深浅涂抹感，极大地提升白底上的质感。
    cv::Mat blurred;
    cv::GaussianBlur(mat, blurred, cv::Size(31, 31), 15);
    
    // 按 70% 马赛克方块 + 30% 模糊背景进行融合
    cv::addWeighted(result, 0.7, blurred, 0.3, 0, result);

    // 将结果包装为 QImage，再生成 QBrush
    QImage mosaicImage(result.data, result.cols, result.rows, result.step, QImage::Format_RGBA8888);
    QPixmap mosaicPixmap = QPixmap::fromImage(mosaicImage.copy());

    QBrush brush(mosaicPixmap);

    // 配置画笔，用于使用纹理刷沿轨迹进行绘制
    QPen p;
    p.setBrush(brush);
    p.setWidth(blockWidth);
    p.setCapStyle(Qt::SquareCap);
    p.setJoinStyle(Qt::BevelJoin);
    p.setStyle(Qt::SolidLine);
    pen_ = p;
}

void DrawMode::setText(const QRectF &rect, const QString& text)
{
    textRect_ = rect;
    text_ = text;
}

void DrawMode::setBitmap(const QImage &image, const QRect &rect)
{
    shape_ = Bitmap;
    bitmap_ = image;
    bitmapRect_ = rect;
    start_ = rect.topLeft();
    end_ = rect.bottomRight();
}

void DrawMode::updateCursor()
{
    if (shape_ == Mosaic) {
        cursor_ = createMosaicCursor();
    } else if (shape_ == PolyLine) {
        cursor_ = createPolyLineCursor();
    } else if (shape_ == Text) {
        cursor_ = Qt::IBeamCursor;
    } else if (shape_ == None) {
        cursor_ = Qt::SizeAllCursor;
    } else {
        cursor_ = Qt::CrossCursor;
    }
}

void DrawMode::clear()
{
    start_ = QPoint(0, 0);
    end_ = QPoint(0, 0);
    points_.clear();
    text_.clear();
    bitmap_ = QImage();
    bitmapRect_ = QRect();
}

void DrawMode::draw(QPainter &painter)
{
    if (!isValid()) {
        return;
    }

    initPainter(painter);
    switch (shape_) {
    case PolyLine:
    case Mosaic:
        drawPolyLine(points_, painter);
        return;
    case Line:
        drawLine(start_, end_, painter);
        return;
    case Arrow:
        drawArrows(start_, end_, painter);
        return;
    case Rectangle:
        drawRect(start_, end_, painter);
        return;
    case Ellipse:
        drawEllipse(start_, end_, painter);
        return;
    case Text:
        drawText(textRect_, text_, painter);
        return;
    case Bitmap:
        if (!bitmap_.isNull() && bitmapRect_.isValid()) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(bitmapRect_, bitmap_);
        }
        return;
    default:
        return;
    }
}

void DrawMode::initPainter(QPainter& painter)
{
    painter.setPen(pen_);
    painter.setBrush(brush_);
    painter.setFont(font_);
    painter.setBrushOrigin(brushOrigin_);
    painter.setRenderHint(QPainter::Antialiasing, true);
}

void DrawMode::drawPolyLine(const QVector<QPoint> &points, QPainter& painter)
{
    // 绘制折线
    QPolygon polygon;
    polygon.append(start_);
    polygon.append(points);
    polygon.append(end_);
    painter.drawPolyline(polygon);
}

void DrawMode::drawLine(const QPoint& startPoint, const QPoint& endPoint, QPainter& painter)
{
    // 绘制直线
    painter.drawLine(startPoint, endPoint);
}

void DrawMode::drawArrows(const QPoint& startPoint, const QPoint& endPoint, QPainter &painter)
{
    // 箭头部分三角形的腰长
    double par = pen_.width() * 6;
    double slopy = atan2((endPoint.y() - startPoint.y()), (endPoint.x() - startPoint.x()));
    double cos_y = cos(slopy);
    double sin_y = sin(slopy);
    QPoint head_point1 = QPoint(endPoint.x() + int(-par*cos_y - (par / 2.0 * sin_y)), endPoint.y() + int(-par*sin_y + (par / 2.0 * cos_y)));
    QPoint head_point2 = QPoint(endPoint.x() + int(-par*cos_y + (par / 2.0 * sin_y)), endPoint.y() - int(par / 2.0*cos_y + par * sin_y));
    QPoint head_points[3] = { endPoint, head_point1, head_point2 };
    // 绘制箭头部分
    painter.drawPolygon(head_points, 3);
    // 计算箭身部分
    int offset_x = int(par*sin_y / 3);
    int offset_y = int(par*cos_y / 3);
    QPoint body_point1, body_point2;
    body_point1 = QPoint(endPoint.x() + int(-par*cos_y - (par / 2.0*sin_y)) + offset_x, endPoint.y() + int(-par*sin_y + (par / 2.0*cos_y)) - offset_y);
    body_point2 = QPoint(endPoint.x() + int(-par*cos_y + (par / 2.0*sin_y) - offset_x), endPoint.y() - int(par / 2.0*cos_y + par*sin_y) + offset_y);
    QPoint body_points[3] = { startPoint, body_point1, body_point2 };
    // 绘制箭身部分
    painter.drawPolygon(body_points, 3);
}

void DrawMode::drawRect(const QPoint &startPoint, const QPoint &endPoint, QPainter &painter) 
{
    // 绘制矩形
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(startPoint, endPoint));
}

void DrawMode::drawEllipse(const QPoint &startPoint, const QPoint &endPoint, QPainter &painter) 
{
    // 绘制椭圆
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRect(startPoint, endPoint));
}

void DrawMode::drawText(const QPoint& startPoint, const QString& text, QPainter& painter)
{
    painter.drawStaticText(startPoint, QStaticText(text));
}

void DrawMode::drawText(const QRectF &rectangle, const QString& text, QPainter& painter)
{
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setDefaultFont(font_);
    document.setPlainText(text);
    document.setTextWidth(-1);
    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat format;
    format.setForeground(pen_.color());
    cursor.mergeCharFormat(format);
    painter.save();
    painter.translate(rectangle.topLeft());
    document.drawContents(&painter);
    painter.restore();
}

////////////////////////////////////////////////////////////////////////////
Drawer::Drawer(QWidget* parent)
    : QObject(parent), parent_(parent), isPressed_(false), isEnabled_(false), textEdit_(nullptr)
{
    parent_->installEventFilter(this);
}

Drawer::~Drawer()
{
    // 防止drawer销毁后cursor没有重置
    setMode(DrawMode(DrawMode::None));
}

QWidget* Drawer::parentWidget()
{
    return parent_;
}

void Drawer::setEnable(bool enable)
{
    isEnabled_ = enable;
}

bool Drawer::enable() const
{
    return isEnabled_;
}

void Drawer::setMode(const DrawMode &drawMode)
{
    saveText();
    drawMode_ = drawMode;
    if (parent_) {
        if (isDraw()) {
            parent_->setCursor(drawMode_.cursor());
        } else {
            parent_->setCursor(Qt::ArrowCursor);
        }
    }
}

const DrawMode& Drawer::mode() const
{
    return drawMode_;
}

bool Drawer::isDraw() const
{
    return isEnabled_ && !drawMode_.isNone();
}

void Drawer::undo()
{
    if (textEdit_ && textEdit_->isVisible()) {
        cancelText();
        return;
    }
    drawStartPos_ = QPoint(0, 0);
    drawEndPos_ = QPoint(0, 0);
    if (!undoHistory_.isEmpty()) {
        UndoState previous = undoHistory_.takeLast();
        if (previous.replacesModes) {
            drawModeCache_ = std::move(previous.modes);
            drawRect_ = previous.drawRect;
            if (previous.onUndo) previous.onUndo();
        } else if (!drawModeCache_.isEmpty()) {
            drawModeCache_.pop_back();
        }
        parent_->update();
    }
}

void Drawer::rememberState(bool replacesModes, std::function<void()> onUndo)
{
    undoHistory_.push_back({replacesModes,
                            replacesModes ? drawModeCache_ : QList<DrawMode>{},
                            replacesModes ? drawRect_ : QRect{},
                            std::move(onUndo)});
}

void Drawer::pushBitmap(const QImage &image, const QRect &rect)
{
    if (image.isNull() || !rect.isValid()) {
        return;
    }
    DrawMode mode(DrawMode::Bitmap);
    mode.setBitmap(image, rect);
    rememberState();
    drawModeCache_.push_back(mode);
    parent_->update();
}

void Drawer::replaceWithBitmap(const QImage &image, const QRect &rect,
                               std::function<void()> onUndo)
{
    if (image.isNull() || !rect.isValid()) return;
    rememberState(true, std::move(onUndo));
    drawModeCache_.clear();
    DrawMode mode(DrawMode::Bitmap);
    mode.setBitmap(image, rect);
    drawModeCache_.push_back(mode);
    parent_->update();
}

void Drawer::clearForTransformation(std::function<void()> onUndo)
{
    rememberState(true, std::move(onUndo));
    drawModeCache_.clear();
    parent_->update();
}

void Drawer::clearHistory()
{
    drawModeCache_.clear();
    undoHistory_.clear();
    drawMode_.clear();
    drawStartPos_ = QPoint(0, 0);
    drawEndPos_ = QPoint(0, 0);
    parent_->update();
}

void Drawer::onPaint(QPainter &painter)
{
    for (DrawMode &cachedMode : drawModeCache_) {
        cachedMode.draw(painter);
    }

    if (!isDraw()) {
        return;
    }

    drawMode_.setPos(drawStartPos_, drawEndPos_);
    drawMode_.draw(painter);
}

void Drawer::showTextEdit(const QPoint &pos)
{
    if (!textEdit_) {
        textEdit_ = new TextEdit(parent_);
        connect(textEdit_, &TextEdit::commitRequested, this, &Drawer::saveText);
        connect(textEdit_, &TextEdit::cancelRequested, this, &Drawer::cancelText);
    }

    textEdit_->setStyle(drawMode_.font(), drawMode_.pen().color());
    textEdit_->move(pos - QPoint(2, 12));
    textEdit_->show();
    textEdit_->setFocus();
}

bool Drawer::saveText()
{
    if (textEdit_ && textEdit_->isVisible()) {
        const bool hadFocus = QApplication::focusWidget() == textEdit_;
        const QString text = textEdit_->toPlainText();
        if (!text.trimmed().isEmpty()) {
            QPoint start = textEdit_->startCursorPoint();
            drawMode_.setText(QRectF(start.x(), start.y(), textEdit_->width(), textEdit_->height()), text);
            rememberState();
            drawModeCache_.push_back(drawMode_);
        }
        drawMode_.clear();
        textEdit_->clear();
        textEdit_->setVisible(false);
        if (hadFocus) parent_->setFocus();
        parent_->update();
        return true;
    }
    return false;
}

void Drawer::cancelText()
{
    if (!textEdit_ || !textEdit_->isVisible()) return;
    const bool hadFocus = QApplication::focusWidget() == textEdit_;
    textEdit_->clear();
    textEdit_->hide();
    if (hadFocus) parent_->setFocus();
    drawMode_.clear();
    parent_->update();
}

void Drawer::setDrawRect(const QRect &rect)
{
    drawRect_ = rect;
}

void Drawer::drawPixmap(QPixmap &pixmap, const QPoint &offset)
{
    saveText();
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    painter.translate(-offset);
    
    for (DrawMode &cachedMode : drawModeCache_) {
        cachedMode.draw(painter);
    }
    
    if (isDraw()) {
        drawMode_.setPos(drawStartPos_, drawEndPos_);
        drawMode_.draw(painter);
    }
}

bool Drawer::eventFilter(QObject* watched, QEvent* event)
{
    if (parent_ != watched) {
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress:
        return onMousePressEvent(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
        return onMouseReleaseEvent(static_cast<QMouseEvent*>(event));
    case QEvent::MouseMove:
        return onMouseMoveEvent(static_cast<QMouseEvent*>(event));
    default:
        return false;
    }
}

bool Drawer::onMousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        isPressed_ = true;
        drawStartPos_ = e->pos();
        drawEndPos_ = e->pos();
        // Reset background image for new stroke (e.g. Mosaic)
        bkImage_ = QImage();

        // 性能优化：马赛克纹理初始化非常耗时（涉及到全屏grab和OpenCV的resize与blur操作）。
        // 如果放在 onMouseMoveEvent 里，会导致鼠标按下后第一次移动瞬间发生严重卡顿。
        // 所以我们提前在鼠标按下时（且确实是处于马赛克模式下）就完成初始化。
        if (isDraw() && drawMode_.shape() == DrawMode::Mosaic) {
            QRect targetRect = parentWidget()->rect();
            if (drawRect_.isValid() && targetRect.contains(drawRect_)) {
                targetRect = drawRect_;
            }
            bkImage_ = parentWidget()->grab(targetRect).toImage();
            drawMode_.initMosaicBrush(bkImage_, targetRect.topLeft());
        }
    }
    return false;
}

bool Drawer::onMouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        // 绘制文本
        if (isDraw()) {
            if (!saveText()) {
                if (drawMode_.shape() == DrawMode::Text) {
                    showTextEdit(e->pos());
                }
            }
        }

        // 当前绘制结束，存档、清理
        if (isPressed_) {
            drawMode_.setPos(drawStartPos_, e->pos());
            if (drawMode_.isPathShape()) {
                drawMode_.addPos(e->pos());
            }

            if (isEnabled_ && drawMode_.isValid()) {
                rememberState();
                drawModeCache_.push_back(drawMode_);
            }
            drawMode_.clear();
            parent_->update();
        }

        drawStartPos_ = QPoint(0, 0);
        drawEndPos_ = QPoint(0, 0);
        isPressed_ = false;
        bkImage_ = QImage();
    }
    return false;
}

bool Drawer::onMouseMoveEvent(QMouseEvent *e)
{
    if (isPressed_ && isDraw()) {
        // 进入绘制模式
        drawEndPos_ = e->pos();

        if (drawMode_.isPathShape()) {
            // 注意：马赛克纹理笔刷已经在 onMousePressEvent 中初始化完毕，
            // 这里仅记录轨迹点。
            drawMode_.addPos(e->pos());
        }

        parent_->update();
        return true;
    }
    return false;
}
