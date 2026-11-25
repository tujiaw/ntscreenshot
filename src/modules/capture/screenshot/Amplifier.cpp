#include "Amplifier.h"

#include <QCoreApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "core/platform/Util.h"

namespace {
const QSize ImageSize(30, 22);
constexpr int Multiple = 4;
constexpr int ContentPadding = 6;
constexpr int RowGap = 2;
const QSize Offset(8, 27);

QFont infoFont(const QWidget* widget)
{
    QFont font = widget->font();
    font.setPixelSize(Util::scaleSize(12));
    return font;
}
}

AmplifierWidget::AmplifierWidget(const std::shared_ptr<QPixmap>& originPainting, QWidget* parent)
    : QWidget(parent), originPainting_(originPainting)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::Tool | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    updateMetrics();
    hide();

    // Cache the image once; mouse movement must not convert the full screenshot.
    if (originPainting_ && !originPainting_->isNull()) {
        originImage_ = originPainting_->toImage();
    }
}

QString AmplifierWidget::getCursorPointColor() const
{
    if (isRgbColor_) {
        return QStringLiteral("RGB(%1, %2, %3)")
            .arg(cursorPointColor_.red()).arg(cursorPointColor_.green()).arg(cursorPointColor_.blue());
    }
    return cursorPointColor_.name().toUpper();
}

QString AmplifierWidget::modeText(bool smart) const
{
    // Keep the existing translations; render their shortcut as a separate keycap.
    QString text = smart ? QCoreApplication::translate("App", "模式: 智能吸附 [ALT]")
                         : QCoreApplication::translate("App", "模式: 窗口检测 [ALT]");
    text.remove(QStringLiteral(" [ALT]"));
    return text;
}

void AmplifierWidget::updateMetrics()
{
    const QFontMetrics metrics(infoFont(this));
    const int padding = Util::scaleSize(ContentPadding);
    const int gap = Util::scaleSize(RowGap);
    const int swatchSize = Util::scaleSize(12);
    const int keyWidth = metrics.horizontalAdvance(QStringLiteral("ALT")) + Util::scaleSize(8);
    int contentWidth = qMax(Util::scaleSize(ImageSize.width() * Multiple),
                           metrics.horizontalAdvance(QStringLiteral("RGB(255, 255, 255)")) + swatchSize + gap * 2);
    contentWidth = qMax(contentWidth, qMax(metrics.horizontalAdvance(modeText(false)),
                                          metrics.horizontalAdvance(modeText(true))) + keyWidth + gap * 2);
    const QPoint globalPosition = QCursor::pos();
    contentWidth = qMax(contentWidth, metrics.horizontalAdvance(
        QStringLiteral("X %1   Y %2").arg(globalPosition.x()).arg(globalPosition.y())));
    imageHeight_ = Util::scaleSize(ImageSize.height() * Multiple);
    rowHeight_ = qMax(metrics.height(), Util::scaleSize(16));
    const QSize cardSize(contentWidth + 2 * padding,
                         imageHeight_ + 3 * rowHeight_ + 2 * gap + 2 * padding);
    if (size() != cardSize) setFixedSize(cardSize);
}

void AmplifierWidget::onSizeChanged(int, int)
{
    const QPoint position = QCursor::pos();
    onPositionChanged(position.x(), position.y());
}

void AmplifierWidget::onPositionChanged(int x, int y)
{
    cursorPoint_ = QPoint(x, y);
    updateMetrics();

    const int offsetX = Util::scaleSize(Offset.width());
    const int offsetY = Util::scaleSize(Offset.height());
    QPoint destination(x + offsetX, y + offsetY);
    if (parentWidget()) {
        const QSize bounds = parentWidget()->size();
        if (destination.x() + width() > bounds.width()) destination.setX(x - offsetX - width());
        if (destination.y() + height() > bounds.height()) destination.setY(y - offsetY - height());
        destination.setX(qBound(0, destination.x(), qMax(0, bounds.width() - width())));
        destination.setY(qBound(0, destination.y(), qMax(0, bounds.height() - height())));
    }
    move(destination);
    update();
}

void AmplifierWidget::paintEvent(QPaintEvent*)
{
    const int padding = Util::scaleSize(ContentPadding);
    const int gap = Util::scaleSize(RowGap);
    const qreal radius = Util::scaleSize(8);
    const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor background(0, 0, 0, 225);
    const QColor border(QStringLiteral("#454545"));
    const QColor keyBackground(QStringLiteral("#303030"));

    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(surface, radius, radius);
    QPainterPath surfaceClip;
    surfaceClip.addRoundedRect(surface, radius, radius);
    painter.setClipPath(surfaceClip);

    const QRect preview(0, 0, width(), imageHeight_);
    // Widen the sampled area with the card, keeping the same pixel magnification
    // and a compact preview height even for longer English mode labels.
    const QSize sampleSize(qMax(ImageSize.width(), qRound(qreal(preview.width()) / Util::scaleSize(Multiple))),
                           ImageSize.height());
    painter.save();
    painter.fillRect(preview, keyBackground);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const qreal dpr = originPainting_ ? originPainting_->devicePixelRatioF() : 1.0;
    const QPoint physicalPoint = cursorPoint_ * dpr;
    if (!originImage_.isNull() && originImage_.rect().contains(physicalPoint)) {
        cursorPointColor_ = originImage_.pixelColor(physicalPoint);
    } else {
        cursorPointColor_ = QColor(Qt::transparent);
    }
    if (originPainting_ && !originPainting_->isNull()) {
        // Leave off-screen pixels blank instead of shifting the sampled pixel away
        // from the crosshair at screenshot edges.
        const QRect source(physicalPoint - QPoint(sampleSize.width() / 2, sampleSize.height() / 2), sampleSize);
        painter.drawPixmap(preview, *originPainting_, source);
    }

    const qreal pixelWidth = qreal(preview.width()) / sampleSize.width();
    const qreal pixelHeight = qreal(preview.height()) / sampleSize.height();
    const QRectF selectedPixel(preview.x() + (sampleSize.width() / 2) * pixelWidth,
                               preview.y() + (sampleSize.height() / 2) * pixelHeight,
                               pixelWidth, pixelHeight);
    const QPointF center = selectedPixel.center();
    painter.setPen(QPen(crossColor_, pixelWidth, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(QPointF(center.x(), preview.top()), QPointF(center.x(), preview.bottom()));
    painter.setPen(QPen(crossColor_, pixelHeight, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(QPointF(preview.left(), center.y()), QPointF(preview.right(), center.y()));
    painter.fillRect(selectedPixel, cursorPointColor_);
    painter.restore();
    painter.setPen(QPen(border, 1));
    painter.drawLine(QPointF(0, imageHeight_ + 0.5), QPointF(width(), imageHeight_ + 0.5));

    painter.setFont(infoFont(this));
    const int left = padding;
    const int contentWidth = width() - 2 * padding;
    int y = preview.y() + preview.height() + padding;
    const QPoint globalPosition = QCursor::pos();
    painter.setPen(Qt::white);
    painter.drawText(QRect(left, y, contentWidth, rowHeight_), Qt::AlignCenter,
                     QStringLiteral("X %1   Y %2").arg(globalPosition.x()).arg(globalPosition.y()));
    y += rowHeight_ + gap;

    const int swatchSize = Util::scaleSize(12);
    const QString colorText = getCursorPointColor();
    const int colorTextWidth = painter.fontMetrics().horizontalAdvance(colorText);
    const int colorInset = swatchSize + 2 * gap;
    const int colorLeft = left + (contentWidth - colorInset - colorTextWidth) / 2;
    const QRectF swatch(colorLeft, y + (rowHeight_ - swatchSize) / 2.0, swatchSize, swatchSize);
    painter.setPen(QPen(border, 1));
    painter.setBrush(cursorPointColor_);
    painter.drawRoundedRect(swatch, Util::scaleSize(4), Util::scaleSize(4));
    painter.setPen(Qt::white);
    painter.drawText(QRect(colorLeft + colorInset, y, colorTextWidth, rowHeight_),
                     Qt::AlignCenter, colorText);
    y += rowHeight_ + gap;

    const int keyWidth = painter.fontMetrics().horizontalAdvance(QStringLiteral("ALT")) + Util::scaleSize(8);
    const QString mode = modeText(useOpenCVMode_);
    const int modeWidth = painter.fontMetrics().horizontalAdvance(mode);
    const int modeLeft = left + (contentWidth - modeWidth - 2 * gap - keyWidth) / 2;
    const QRectF keycap(modeLeft + modeWidth + 2 * gap, y, keyWidth, rowHeight_);
    painter.setPen(QPen(border, 1));
    painter.setBrush(keyBackground);
    painter.drawRoundedRect(keycap, Util::scaleSize(5), Util::scaleSize(5));
    painter.setPen(Qt::white);
    painter.drawText(keycap, Qt::AlignCenter, QStringLiteral("ALT"));
    painter.drawText(QRect(modeLeft, y, modeWidth, rowHeight_), Qt::AlignCenter, mode);
    painter.setClipping(false);
    painter.setPen(QPen(border, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(surface, radius, radius);
}
