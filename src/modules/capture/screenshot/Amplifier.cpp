#include "Amplifier.h"
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include "core/foundation/Constants.h"
#include "core/platform/Util.h"

const QSize IMAGE_SIZE(30, 22);
const int MULTIPLE = 4;
const QSize OFFSET(8, 27);

AmplifierWidget::AmplifierWidget(const std::shared_ptr<QPixmap> &originPainting, QWidget *parent) 
    : QWidget(parent), originPainting_(originPainting)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::Tool | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    
    // 使用通用DPI适配方法计算尺寸
    int scaledImageWidth = Util::scaleSize(IMAGE_SIZE.width() * MULTIPLE);
    int scaledImageHeight = Util::scaleSize(IMAGE_SIZE.height() * MULTIPLE);
    imageHeight_ = scaledImageHeight;
    int scaledBottomHeight = Util::scaleSize(50); // 增加底部高度以容纳3行文字
    setFixedSize(scaledImageWidth, imageHeight_ + scaledBottomHeight);
    hide();

    // 性能优化：缓存一次 QImage，避免 paintEvent 高频调用 toImage()
    if (originPainting_ && !originPainting_->isNull()) {
        originImage_ = originPainting_->toImage();
    }
}

QString AmplifierWidget::getCursorPointColor() const
{
    QString colorInfo;
    if (isRgbColor_) {
        colorInfo = QString("RGB(%1, %2, %3)").arg(cursorPointColor_.red()).arg(cursorPointColor_.green()).arg(cursorPointColor_.blue());
    } else {
        colorInfo = cursorPointColor_.name().toUpper();
    }
    return colorInfo;
}

void AmplifierWidget::onSizeChanged(int w, int h) 
{
    QPoint p(QCursor::pos());
    onPositionChanged(p.x(), p.y());
}

void AmplifierWidget::onPositionChanged(int x, int y) 
{
    cursorPoint_ = QPoint(x, y);
    
    // 动态调整DPI以适应跨屏幕移动
    int scaledImageWidth = Util::scaleSize(IMAGE_SIZE.width() * MULTIPLE);
    int scaledImageHeight = Util::scaleSize(IMAGE_SIZE.height() * MULTIPLE);
    if (imageHeight_ != scaledImageHeight || width() != scaledImageWidth) {
        imageHeight_ = scaledImageHeight;
        int scaledBottomHeight = Util::scaleSize(50);
        setFixedSize(scaledImageWidth, imageHeight_ + scaledBottomHeight);
    }
    
    // 使用通用DPI适配方法缩放偏移量
    int offsetX = Util::scaleSize(OFFSET.width());
    int offsetY = Util::scaleSize(OFFSET.height());
    QPoint destPos(x + offsetX, y + offsetY);

    /// 超出屏幕检测
    const QSize& parent_size = parentWidget()->size();

    if (destPos.x() + width() > parent_size.width()) {
        destPos.setX(x - offsetX - width());
    }

    if (destPos.y() + height() > parent_size.height()) {
        destPos.setY(y - offsetY - height());
    }

    move(destPos);
    update();
}

//// 绘制鼠标拖拽时选区矩形的右下顶点的放大图;
void AmplifierWidget::paintEvent(QPaintEvent *) 
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    /// 绘制背景
    painter.fillRect(rect(), QColor(0, 0, 0, 200));

    /// 绘制放大图：避免 copy().scaled() 的中间 QPixmap 分配，直接绘制源矩形到目标矩形
    if (originPainting_ && !originPainting_->isNull()) {
        const int mx = IMAGE_SIZE.width() / 2;
        const int my = IMAGE_SIZE.height() / 2;

        const int imgW = originPainting_->width();
        const int imgH = originPainting_->height();

        double dpr = originPainting_ ? originPainting_->devicePixelRatioF() : 1.0;
        int sx = cursorPoint_.x() * dpr - mx;
        int sy = cursorPoint_.y() * dpr - my;
        sx = qBound(0, sx, qMax(0, imgW - IMAGE_SIZE.width()));
        sy = qBound(0, sy, qMax(0, imgH - IMAGE_SIZE.height()));
        const QRect srcRect(sx, sy, IMAGE_SIZE.width(), IMAGE_SIZE.height());

        const QRect dstRect(1, 1, width() - 2, imageHeight_ - 2);
        painter.drawPixmap(dstRect, *originPainting_, srcRect);
    }

    /// 当前鼠标像素值的RGB信息：使用缓存的 QImage
    QPoint physicalPoint;
    double dpr = 1.0;
    if (originPainting_) {
        dpr = originPainting_->devicePixelRatioF();
    } else {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (screen) {
            dpr = screen->devicePixelRatio();
        }
    }
    physicalPoint = cursorPoint_ * dpr;
    
    if (!originImage_.isNull() && originImage_.rect().contains(physicalPoint)) {
        cursorPointColor_ = QColor::fromRgba(originImage_.pixel(physicalPoint));
    } else {
        cursorPointColor_ = QColor(Qt::transparent);
    }

    /// 绘制十字
    // 使用通用DPI适配方法缩放画笔宽度
    int penWidth = Util::scaleSize(MULTIPLE);
    painter.setPen(QPen(QColor(0, 180, 255, 180), penWidth));
    /// 竖线;
    int penOffset = Util::scaleSize(MULTIPLE);
    painter.drawLine(QPoint((width() >> 1) + 1, 0), QPoint((width() >> 1) + 1, imageHeight_ - penOffset));
    /// 横线;
    painter.drawLine(QPoint(0, (imageHeight_ >> 1) + 1), QPoint(width(), (imageHeight_ >> 1) + 1));

    /// 绘制中间颜色块
    int colorBlockSize = Util::scaleSize(6);
    int colorBlockOffset = Util::scaleSize(2);
    painter.fillRect((width() >> 1) - colorBlockOffset, (imageHeight_ >> 1) - colorBlockOffset, 
                     colorBlockSize, colorBlockSize, cursorPointColor_);
    /// 绘制中间颜色块边框
    painter.setPen(QPen(Qt::black, Util::scaleSize(1)));
    int borderOffset = Util::scaleSize(3);
    int borderSize = Util::scaleSize(7);
    painter.drawRect((width() >> 1) - borderOffset, (imageHeight_ >> 1) - borderOffset, 
                     borderSize, borderSize);

    /// 绘制大图内边框
    int innerBorderWidth = Util::scaleSize(2);
    int innerBorderOffset = Util::scaleSize(2);
    painter.setPen(QPen(Qt::white, innerBorderWidth));
    painter.drawRect(innerBorderOffset, innerBorderOffset, 
                    width() - innerBorderOffset * 2, imageHeight_ - innerBorderOffset * 2);

    /// 绘制外边框
    painter.setPen(QPen(Qt::black, Util::scaleSize(1)));
    painter.drawRect(0, 0, width() - 1, height() - 1);

    /// 当前选中矩形的宽高信息;
    QString posInfo = QStringLiteral("%1 x %2").arg(QCursor::pos().x()).arg(QCursor::pos().y());

    QFont fixedFont = painter.font();
    fixedFont.setPixelSize(Util::scaleSize(12));
    painter.setFont(fixedFont);

    // 绘制坐标轴相关数据
    painter.setPen(Qt::white);
    QFontMetrics metrics(fixedFont);
    const int space = Util::scaleSize(3);
    int x = space;
    int y = imageHeight_ + metrics.ascent();
    
    painter.drawText(QPoint(x, y), posInfo);
    y += metrics.lineSpacing();
    painter.drawText(QPoint(x, y), getCursorPointColor());
    y += metrics.lineSpacing();
    painter.drawText(QPoint(x, y), useOpenCVMode_ ? QStringLiteral("模式: 智能吸附 [ALT]") : QStringLiteral("模式: 窗口检测 [ALT]"));
}
