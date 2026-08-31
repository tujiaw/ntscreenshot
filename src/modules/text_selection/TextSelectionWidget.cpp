#include "TextSelectionWidget.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "TextSelectionToolbar.h"
#include "core/platform/Util.h"

TextSelectionWidget::TextSelectionWidget(QWidget *parent)
    : QWidget(parent)
    , state_(SelectionState::Idle)
    , toolbar_(std::make_unique<TextSelectionToolbar>(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);

    QRect desktopRect = Util::desktopRect();
    double dpr = devicePixelRatioF();

#ifdef Q_OS_WIN
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setGeometry(desktopRect.x() / dpr, desktopRect.y() / dpr, desktopRect.width() / dpr, desktopRect.height() / dpr);
    show();
    SetWindowPos((HWND)winId(), HWND_TOPMOST, desktopRect.x(), desktopRect.y(), desktopRect.width(), desktopRect.height(), SWP_SHOWWINDOW);
#else
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    setGeometry(desktopRect.x() / dpr, desktopRect.y() / dpr, desktopRect.width() / dpr, desktopRect.height() / dpr);
    show();
    activateWindow();
#endif

    connect(toolbar_.get(), &TextSelectionToolbar::sigActionTriggered, this, &TextSelectionWidget::onToolbarActionTriggered);
    toolbar_->hide();

    raise();
    activateWindow();
    setFocus();
}

void TextSelectionWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        emit sigClose();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (toolbar_) {
        toolbar_->hide();
    }

    startPoint_ = event->pos();
    selectionRect_ = QRect(startPoint_, startPoint_);
    state_ = SelectionState::Selecting;
    update();

    QWidget::mousePressEvent(event);
}

void TextSelectionWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (state_ == SelectionState::Selecting) {
        selectionRect_ = normalizedSelection(event->pos());
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void TextSelectionWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (state_ == SelectionState::Selecting) {
        selectionRect_ = normalizedSelection(event->pos());
        if (hasValidSelection()) {
            state_ = SelectionState::Selected;
            showToolbar();
        } else {
            resetSelection();
        }
        update();
    }

    QWidget::mouseReleaseEvent(event);
}

void TextSelectionWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit sigClose();
        return;
    }

    QWidget::keyPressEvent(event);
}

void TextSelectionWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(0, 0, 0, 72));

    if (!selectionRect_.isNull()) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(selectionRect_, Qt::transparent);

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QPen borderPen(QColor("#1AAD19"));
        borderPen.setWidth(Util::scaleSize(2));
        painter.setPen(borderPen);
        painter.drawRect(selectionRect_.adjusted(0, 0, -1, -1));
    }

    if (state_ == SelectionState::Idle) {
        const QString hint = QStringLiteral("拖拽选择文本区域，释放后弹出工具条");
        QFont font = painter.font();
        font.setPixelSize(Util::scaleSize(16));
        painter.setFont(font);

        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(hint);
        QRect bubble(
            (width() - textRect.width()) / 2 - Util::scaleSize(18),
            Util::scaleSize(40),
            textRect.width() + Util::scaleSize(36),
            textRect.height() + Util::scaleSize(18));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 140));
        painter.drawRoundedRect(bubble, Util::scaleSize(10), Util::scaleSize(10));

        painter.setPen(Qt::white);
        painter.drawText(bubble, Qt::AlignCenter, hint);
    }
}

void TextSelectionWidget::onToolbarActionTriggered(const QString &actionId, const QString &inputText)
{
    Q_UNUSED(inputText);

    if (!hasValidSelection()) {
        return;
    }

    QRect globalSelection(mapToGlobal(selectionRect_.topLeft()), selectionRect_.size());
    emit sigActionTriggered(actionId, globalSelection);
}

QRect TextSelectionWidget::normalizedSelection(const QPoint &endPoint) const
{
    return QRect(startPoint_, endPoint).normalized();
}

bool TextSelectionWidget::hasValidSelection() const
{
    return selectionRect_.width() >= MIN_SELECTION_EDGE && selectionRect_.height() >= MIN_SELECTION_EDGE;
}

void TextSelectionWidget::resetSelection()
{
    state_ = SelectionState::Idle;
    selectionRect_ = QRect();
    if (toolbar_) {
        toolbar_->hide();
    }
}

void TextSelectionWidget::showToolbar()
{
    if (!toolbar_ || !hasValidSelection()) {
        return;
    }

    toolbar_->showForSelection(selectionRect_, rect());
}
