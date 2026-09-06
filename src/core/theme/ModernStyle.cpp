#include "core/theme/ModernStyle.h"

#include <QPainter>
#include <QStyleOption>

ModernStyle::ModernStyle(QStyle* baseStyle, int radioIndicatorSize)
    : QProxyStyle(baseStyle)
    , radioIndicatorSize_(qMax(1, radioIndicatorSize))
{
}

void ModernStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                                QPainter* painter, const QWidget* widget) const
{
    if (element != PE_IndicatorRadioButton || !option || !painter) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }

    const bool enabled = option->state.testFlag(State_Enabled);
    const bool checked = option->state.testFlag(State_On);
    const bool hovered = option->state.testFlag(State_MouseOver);
    const QColor accent = option->palette.color(enabled ? QPalette::Active : QPalette::Disabled,
                                                QPalette::Highlight);
    const QColor border = checked || hovered
        ? accent
        : option->palette.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Mid);
    QColor background = option->palette.color(enabled ? QPalette::Active : QPalette::Disabled,
                                               QPalette::Base);
    if (hovered && !checked) {
        background = accent;
        background.setAlpha(28);
    }

    const qreal side = qMin(option->rect.width(), option->rect.height());
    QRectF circle(option->rect.center().x() - side / 2.0 + 0.5,
                  option->rect.center().y() - side / 2.0 + 0.5,
                  side - 1.0, side - 1.0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(border, qMax<qreal>(1.0, side * 0.075)));
    painter->setBrush(background);
    painter->drawEllipse(circle);

    if (checked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        const qreal dotRadius = side * 0.2;
        painter->drawEllipse(circle.center(), dotRadius, dotRadius);
    }
    painter->restore();
}

int ModernStyle::pixelMetric(PixelMetric metric, const QStyleOption* option,
                             const QWidget* widget) const
{
    if (metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight) {
        return radioIndicatorSize_;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}
