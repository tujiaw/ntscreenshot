#include "TextSelectionToolbar.h"

#include <QHBoxLayout>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QStyle>

#include "core/theme/OverlayTheme.h"
#include "core/platform/Util.h"

namespace {

// 外围为阴影预留的空间（px，基准 DPI）
constexpr int kShadowPad   = 10;
constexpr int kShadowOffY  = 5;   // 阴影向下偏移
constexpr int kInnerPadH   = 8;   // Pill 内部水平内边距
constexpr int kButtonHeight = 34; // 按钮高度（Google Material 标准）
constexpr int kCornerRadius = 12; // 更克制的小圆角，避免和纯白背景融成一片
constexpr int kGlobalPointGap = 8; // 划词弹框与鼠标之间的距离

QString buttonStyle(bool dark)
{
    // Google Blue: #1a73e8（亮）/ #8ab4f8（暗）
    const QString text    = dark ? "#8ab4f8" : "#1a73e8";
    const QString hover   = dark ? "rgba(138,180,248,20)" : "rgba(26,115,232,18)";
    const QString pressed = dark ? "rgba(138,180,248,38)" : "rgba(26,115,232,36)";

    return QStringLiteral(
        "QPushButton{"
        "  border: none;"
        "  padding: 0 8px;"
        "  background: transparent;"
        "  color: %1;"
        "  font-size: 11px;"
        "  font-weight: 400;"
        "}"
        "QPushButton:hover{ background: %2; }"
        "QPushButton:pressed{ background: %3; }"
    ).arg(text, hover, pressed);
}

QString lineEditStyle(bool dark)
{
    const QString text = dark ? "#e5e7eb" : "#111827";
    const QString placeholder = dark ? "rgba(229,231,235,0.58)" : "rgba(17,24,39,0.46)";
    const QString bg = dark ? "rgba(255,255,255,0.06)" : "rgba(255,255,255,0.82)";
    const QString border = dark ? "rgba(255,255,255,0.12)" : "rgba(15,23,42,0.10)";
    const QString focus = dark ? "#8ab4f8" : "#1a73e8";

    return QStringLiteral(
        "QLineEdit{"
        "  border: 1px solid %1;"
        "  border-radius: 10px;"
        "  padding: 0 12px;"
        "  background: %2;"
        "  color: %3;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus{"
        "  border-color: %4;"
        "}"
        "QLineEdit[expanded=\"true\"]{"
        "  border: 1px solid transparent;"
        "  border-radius: 10px;"
        "  background: transparent;"
        "  padding: 0 12px;"
        "}"
        "QLineEdit[placeholderTextVisible=\"true\"]{"
        "  color: %5;"
        "}"
    ).arg(border, bg, text, focus, placeholder);
}

QString dragHandleStyle(bool dark)
{
    Q_UNUSED(dark);

    return QStringLiteral(
        "QWidget#textSelectionDragHandle{"
        "  border-radius: 8px;"
        "  background: transparent;"
        "}"
    );
}

QPoint clampTopLevelPointToRect(const QPoint &point, const QSize &size, const QRect &rect)
{
    int x = point.x();
    int y = point.y();

    if (x < rect.left()) {
        x = rect.left();
    } else if (x + size.width() > rect.right() + 1) {
        x = rect.right() + 1 - size.width();
    }

    if (y < rect.top()) {
        y = rect.top();
    } else if (y + size.height() > rect.bottom() + 1) {
        y = rect.bottom() + 1 - size.height();
    }

    return QPoint(x, y);
}

bool sameTextSelectionActions(const QList<TextSelectionActionConfig> &a,
                              const QList<TextSelectionActionConfig> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id
            || a[i].label != b[i].label
            || a[i].prompt != b[i].prompt) {
            return false;
        }
    }
    return true;
}

}  // namespace

TextSelectionToolbar::TextSelectionToolbar(QWidget *parent)
    : QWidget(parent)
    , layout_(nullptr)
{
    if (!parent) {
        setWindowFlags(Qt::FramelessWindowHint
                       | Qt::WindowStaysOnTopHint
                       | Qt::Tool
                       | Qt::NoDropShadowWindowHint);
    }
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::ClickFocus);

    layout_ = new QHBoxLayout(this);
    // 边距 = 阴影 + 内边距；底部多留 kShadowOffY 给阴影下移
    Util::scaleLayoutMargins(layout_,
                             kShadowPad + kInnerPadH,
                             kShadowPad,
                             kShadowPad + kInnerPadH,
                             kShadowPad + kShadowOffY);
    layout_->setSpacing(Util::scaleSize(2));

    adjustSize();
    hide();
}

void TextSelectionToolbar::setActions(const QList<TextSelectionActionConfig> &actions)
{
    if (dragHandle_ && sameTextSelectionActions(actions_, actions)) {
        return;
    }
    actions_ = actions;
    rebuildActionButtons();
}

void TextSelectionToolbar::showForSelection(const QRect &selectionRect, const QRect &boundsRect)
{
    resetCompactPresentation();
    if (chatInput_) {
        chatInput_->clear();
    }

    refreshCompactSize();

    const int gap = Util::scaleSize(10);
    int x = selectionRect.left() + (selectionRect.width() - width()) / 2;
    int y = selectionRect.top() - height() - gap;

    if (y < boundsRect.top()) {
        y = selectionRect.bottom() + gap;
    }
    if (x < boundsRect.left() + gap) {
        x = boundsRect.left() + gap;
    }
    if (x + width() > boundsRect.right() - gap + 1) {
        x = boundsRect.right() - width() - gap + 1;
    }
    if (y + height() > boundsRect.bottom() - gap + 1) {
        y = selectionRect.top() - height() - gap;
    }
    if (y < boundsRect.top() + gap) {
        y = boundsRect.top() + gap;
    }

    move(x, y);
    show();
    raise();
}

void TextSelectionToolbar::showNearGlobalPoint(const QPoint &globalPoint)
{
    resetCompactPresentation();
    if (chatInput_) {
        chatInput_->clear();
    }

    refreshCompactSize();

    const int gap = Util::scaleSize(kGlobalPointGap);
    QPoint anchor(globalPoint.x() - width() / 2, globalPoint.y() + gap);
    QRect screenRect = Util::desktopRect();
    if (QScreen *screen = QGuiApplication::screenAt(globalPoint)) {
        screenRect = screen->availableGeometry();
    }

    if (anchor.y() + height() > screenRect.bottom() + 1 - gap) {
        anchor.setY(globalPoint.y() - height() - gap);
    }

    move(clampTopLevelPointToRect(anchor, size(), screenRect));
    show();
    raise();
}

bool TextSelectionToolbar::containsGlobalPoint(const QPoint &globalPoint) const
{
    return geometry().contains(globalPoint);
}

bool TextSelectionToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == dragHandle_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragging_ = true;
                dragOffset_ = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (dragging_ && (mouseEvent->buttons() & Qt::LeftButton)) {
                move(mouseEvent->globalPosition().toPoint() - dragOffset_);
                return true;
            }
            if ((mouseEvent->buttons() & Qt::LeftButton) == 0) {
                dragging_ = false;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragging_ = false;
                return true;
            }
        } else if (event->type() == QEvent::Leave) {
            dragging_ = false;
        } else if (event->type() == QEvent::Paint) {
            auto *paintEvent = static_cast<QPaintEvent*>(event);
            QWidget::eventFilter(watched, event);

            Q_UNUSED(paintEvent);
            QPainter painter(dragHandle_);
            painter.setRenderHint(QPainter::Antialiasing, true);

            const bool dark = OverlayTheme::isDarkTheme();
            const QColor dotColor = dark ? QColor(255, 255, 255, 210) : QColor(17, 24, 39, 184);
            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);

            const int dotSize = qMax(2, Util::scaleSize(2));
            const int gap = qMax(3, Util::scaleSize(3));
            const int rows = 3;
            const int cols = 2;
            const int totalWidth = cols * dotSize + (cols - 1) * gap;
            const int totalHeight = rows * dotSize + (rows - 1) * gap;
            const int startX = (dragHandle_->width() - totalWidth) / 2;
            const int startY = (dragHandle_->height() - totalHeight) / 2;

            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    const int x = startX + col * (dotSize + gap);
                    const int y = startY + row * (dotSize + gap);
                    painter.drawEllipse(QRectF(x, y, dotSize, dotSize));
                }
            }
            return true;
        }
    }

    if (watched == chatInput_) {
        if (event->type() == QEvent::MouseButtonPress) {
            setChatInputExpanded(true);
        } else if (event->type() == QEvent::FocusOut) {
            setChatInputExpanded(false);
        }
    }

    return QWidget::eventFilter(watched, event);
}

void TextSelectionToolbar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool dark = OverlayTheme::isDarkTheme();
    const int sp = Util::scaleSize(kShadowPad);
    const int sy = Util::scaleSize(kShadowOffY);
    const qreal radius = Util::scaleSize(kCornerRadius);

    // Pill 主体区域（阴影内侧）
    const QRectF body(sp, sp, width() - sp * 2, height() - sp - sp - sy);

    // ── Material 多层柔和阴影 ──────────────────────────────
    // 模拟 elevation=3：外层宽而透明（环境光），内层窄而不透明（关键光）
    struct Layer { qreal spread; qreal yOff; int alpha; };
    const Layer layers[] = {
        { qreal(Util::scaleSize(7)), qreal(Util::scaleSize(4)), dark ? 55 : 22 },
        { qreal(Util::scaleSize(4)), qreal(Util::scaleSize(3)), dark ? 48 : 32 },
        { qreal(Util::scaleSize(1)), qreal(Util::scaleSize(2)), dark ? 42 : 44 },
    };

    painter.setPen(Qt::NoPen);
    for (const auto &l : layers) {
        QRectF sr = body.adjusted(-l.spread, -l.spread * 0.5, l.spread, l.spread);
        sr.translate(0, l.yOff);
        painter.setBrush(QColor(0, 0, 0, l.alpha));
        painter.drawRoundedRect(sr, radius + l.spread * 0.35, radius + l.spread * 0.35);
    }

    // ── Pill 背景 ─────────────────────────────────────────
    const QColor bg = dark ? QColor(0x2c, 0x2d, 0x31) : QColor(0xf5, 0xf7, 0xfa);
    const QColor border = dark ? QColor(255, 255, 255, 18) : QColor(15, 23, 42, 22);
    painter.setBrush(bg);
    painter.setPen(QPen(border, 1));
    painter.drawRoundedRect(body, radius, radius);
}

void TextSelectionToolbar::refreshCompactSize()
{
    // 仅在紧凑态（actionContainer 可见）时重新测量，防止首次显示前 sizeHint 未就绪导致宽度偏小
    if (!layout_ || !actionContainer_ || !actionContainer_->isVisible()) {
        return;
    }

    for (QWidget *child : findChildren<QWidget *>()) {
        child->ensurePolished();
    }
    ensurePolished();
    layout_->activate();

    const int minW = layout_->contentsMargins().left()
                     + layout_->contentsMargins().right()
                     + Util::scaleSize(80);
    const int w = qMax(layout_->sizeHint().width(), minW);

    if (w > compactWidth_) {
        compactWidth_ = w;
        setFixedSize(compactWidth_, compactHeight_);
    }
}

void TextSelectionToolbar::resetCompactPresentation()
{
    if (!chatInput_) {
        return;
    }

    const int compactInputWidth = Util::scaleSize(80);

    if (chatInputAnimation_) {
        chatInputAnimation_->stop();
    }

    chatInputExpanded_ = false;
    pendingShowActionsAfterCollapse_ = false;

    if (actionContainer_) {
        actionContainer_->show();
    }

    chatInput_->setMinimumWidth(compactInputWidth);
    chatInput_->setMaximumWidth(compactInputWidth);
    refreshChatInputStyle(false);

    if (compactWidth_ > 0 && compactHeight_ > 0) {
        setFixedSize(compactWidth_, compactHeight_);
    } else {
        adjustSize();
    }
}

void TextSelectionToolbar::rebuildActionButtons()
{
    if (!layout_) {
        return;
    }

    if (chatInputAnimation_) {
        chatInputAnimation_->stop();
        delete chatInputAnimation_;
        chatInputAnimation_ = nullptr;
    }

    dragHandle_ = nullptr;
    actionContainer_ = nullptr;
    actionLayout_ = nullptr;
    chatInput_ = nullptr;
    compactWidth_ = 0;
    compactHeight_ = 0;
    chatInputExpanded_ = false;
    pendingShowActionsAfterCollapse_ = false;

    while (layout_->count() > 0) {
        QLayoutItem *item = layout_->takeAt(0);
        if (QWidget *widget = item->widget()) {
            // 同步删除：deleteLater 会在下一轮事件循环才销毁，工具栏仍可见时再次
            // rebuild 会导致旧子控件与新控件重叠，出现重影与布局错乱。
            delete widget;
        }
        delete item;
    }

    dragHandle_ = new QWidget(this);
    dragHandle_->setObjectName(QStringLiteral("textSelectionDragHandle"));
    dragHandle_->setFixedSize(Util::scaleSize(22), Util::scaleSize(28));
    dragHandle_->setCursor(Qt::SizeAllCursor);
    dragHandle_->installEventFilter(this);
    dragHandle_->setStyleSheet(dragHandleStyle(OverlayTheme::isDarkTheme()));
    layout_->addWidget(dragHandle_, 0, Qt::AlignVCenter);

    actionContainer_ = new QWidget(this);
    actionLayout_ = new QHBoxLayout(actionContainer_);
    actionLayout_->setContentsMargins(0, 0, 0, 0);
    actionLayout_->setSpacing(Util::scaleSize(2));

    addActionButton(actionLayout_, QStringLiteral("copy"), QStringLiteral("复制"));

    const QList<TextSelectionActionConfig> effectiveActions = actions_;
    for (const TextSelectionActionConfig &action : effectiveActions) {
        if (action.id.trimmed().isEmpty() || action.label.trimmed().isEmpty()) {
            continue;
        }
        addActionButton(actionLayout_, action.id.trimmed(), action.label.trimmed());
    }

    layout_->addWidget(actionContainer_);
    addChatInput(layout_);

    for (QWidget *child : findChildren<QWidget *>()) {
        child->ensurePolished();
    }
    ensurePolished();
    layout_->activate();

    // 高度从常量直接推导，避免 sizeHint 在首次渲染前返回偏小值
    const int topMargin = layout_->contentsMargins().top();
    const int botMargin = layout_->contentsMargins().bottom();
    compactHeight_ = topMargin + Util::scaleSize(kButtonHeight) + botMargin;

    const int defaultInputWidth = Util::scaleSize(80);
    const QSize layoutHint = layout_->sizeHint();
    compactWidth_ = qMax(layoutHint.width(),
                         layout_->contentsMargins().left()
                         + layout_->contentsMargins().right()
                         + defaultInputWidth);

    // 用 setFixedSize 锁定尺寸：展开/收起 chatInput 时 actionContainer 会 hide/show，
    // 若不锁定，布局变化会导致窗口宽度收缩。
    setFixedSize(compactWidth_, compactHeight_);
    resetCompactPresentation();
}

void TextSelectionToolbar::addActionButton(QHBoxLayout *layout,
                                           const QString &actionId,
                                           const QString &text)
{
    auto *btn = new QPushButton(text, this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFlat(true);
    btn->setMinimumHeight(Util::scaleSize(kButtonHeight));
    btn->setStyleSheet(buttonStyle(OverlayTheme::isDarkTheme()));

    connect(btn, &QPushButton::clicked, this, [this, actionId]() {
        emit sigActionTriggered(actionId, QString());
    });

    layout->addWidget(btn);
}

void TextSelectionToolbar::addChatInput(QHBoxLayout *layout)
{
    const int compactInputWidth = Util::scaleSize(80);

    chatInput_ = new QLineEdit(this);
    chatInput_->setPlaceholderText(QStringLiteral("问问AI"));
    chatInput_->setMinimumWidth(compactInputWidth);
    chatInput_->setMaximumWidth(compactInputWidth);
    chatInput_->setFixedHeight(Util::scaleSize(28));
    chatInput_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    chatInput_->setStyleSheet(lineEditStyle(OverlayTheme::isDarkTheme()));
    chatInput_->setClearButtonEnabled(true);
    chatInput_->installEventFilter(this);
    refreshChatInputStyle(false);

    chatInputAnimation_ = new QPropertyAnimation(chatInput_, "maximumWidth", this);
    chatInputAnimation_->setDuration(160);
    chatInputAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(chatInputAnimation_, &QPropertyAnimation::finished, this, [this, compactInputWidth]() {
        if (!chatInput_) {
            return;
        }

        if (pendingShowActionsAfterCollapse_) {
            pendingShowActionsAfterCollapse_ = false;
            if (actionContainer_) {
                actionContainer_->show();
            }
            chatInput_->setMinimumWidth(compactInputWidth);
            chatInput_->setMaximumWidth(compactInputWidth);
            refreshChatInputStyle(false);
        }
    });

    connect(chatInput_, &QLineEdit::returnPressed, this, [this]() {
        if (!chatInput_) {
            return;
        }

        const QString inputText = chatInput_->text().trimmed();
        chatInput_->clear();
        setChatInputExpanded(false);
        emit sigActionTriggered(QStringLiteral("chat"), inputText);
    });
    layout->addWidget(chatInput_);
}

void TextSelectionToolbar::setChatInputExpanded(bool expanded)
{
    if (!chatInput_ || !actionContainer_) {
        return;
    }

    if (chatInputExpanded_ == expanded && (!chatInputAnimation_ || chatInputAnimation_->state() != QAbstractAnimation::Running)) {
        return;
    }

    const int compactInputWidth = Util::scaleSize(80);
    const int currentWidth = qMax(compactInputWidth, chatInput_->width());

    if (!isVisible() || !chatInputAnimation_) {
        chatInputExpanded_ = expanded;
        pendingShowActionsAfterCollapse_ = false;
        if (expanded) {
            if (compactWidth_ <= 0) {
                compactWidth_ = width() > 0 ? width() : QWidget::sizeHint().width();
            }
            actionContainer_->hide();
            const int availableWidth = qMax(compactInputWidth,
                                            compactWidth_
                                            - layout_->contentsMargins().left()
                                            - layout_->contentsMargins().right());
            chatInput_->setMinimumWidth(compactInputWidth);
            chatInput_->setMaximumWidth(availableWidth);
            refreshChatInputStyle(true);
        } else {
            actionContainer_->show();
            chatInput_->setMinimumWidth(compactInputWidth);
            chatInput_->setMaximumWidth(compactInputWidth);
            refreshChatInputStyle(false);
        }
        adjustSize();
        return;
    }

    if (expanded) {
        if (compactWidth_ <= 0) {
            compactWidth_ = width() > 0 ? width() : QWidget::sizeHint().width();
        }

        pendingShowActionsAfterCollapse_ = false;
        chatInputAnimation_->stop();
        chatInputExpanded_ = true;
        actionContainer_->hide();
        const int availableWidth = qMax(compactInputWidth,
                                        compactWidth_
                                        - layout_->contentsMargins().left()
                                        - layout_->contentsMargins().right());
        chatInput_->setMinimumWidth(compactInputWidth);
        chatInput_->setMaximumWidth(currentWidth);
        refreshChatInputStyle(true);
        chatInputAnimation_->setStartValue(currentWidth);
        chatInputAnimation_->setEndValue(availableWidth);
        chatInputAnimation_->start();
    } else {
        chatInputAnimation_->stop();
        chatInputExpanded_ = false;
        pendingShowActionsAfterCollapse_ = true;
        chatInput_->setMinimumWidth(compactInputWidth);
        chatInput_->setMaximumWidth(currentWidth);
        chatInputAnimation_->setStartValue(currentWidth);
        chatInputAnimation_->setEndValue(compactInputWidth);
        chatInputAnimation_->start();
    }
}

void TextSelectionToolbar::refreshChatInputStyle(bool expanded)
{
    if (!chatInput_) {
        return;
    }

    chatInput_->setProperty("expanded", expanded);
    chatInput_->style()->unpolish(chatInput_);
    chatInput_->style()->polish(chatInput_);
    chatInput_->update();
}
