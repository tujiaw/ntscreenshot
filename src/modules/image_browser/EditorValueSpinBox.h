#pragma once
#include "core/theme/ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QSpinBox>
#include <QStyleOptionSpinBox>

namespace ImageBrowser {
// Equal button geometry and mirrored vector chevrons at every application scale.
class EditorValueSpinBox final : public QSpinBox {
public:
    explicit EditorValueSpinBox(QWidget* parent) : QSpinBox(parent) { setControlScale(1); }
    // Color rows use a fixed 78px control; form fields and dialogs follow the layout width.
    void setFixedControlWidth(bool fixed) { fixedWidth_ = fixed; setControlScale(scale_); }
    void setControlScale(double scale) {
        scale_ = scale;
        const int height = qRound(26*scale), buttonHeight = (height-2)/2;
        if (fixedWidth_) setFixedSize(qRound(78*scale), height); else setFixedHeight(height);
        setStyleSheet(QStringLiteral(
            "QSpinBox { padding:0px %1px 0px %2px; }"
            "QSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:%3px; height:%4px; margin:1px 1px 0px 0px; }"
            "QSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:%3px; height:%4px; margin:0px 1px 1px 0px; }"
            "QSpinBox::up-arrow, QSpinBox::down-arrow { image:none; width:0px; height:0px; }")
            .arg(qRound(22*scale)).arg(qRound(8*scale)).arg(qRound(20*scale)).arg(buttonHeight));
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        QSpinBox::paintEvent(event);
        QStyleOptionSpinBox option; initStyleOption(&option);
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        const auto& theme = ThemeManager::tokens();
        for (bool up : {true, false}) {
            const auto control = up ? QStyle::SC_SpinBoxUp : QStyle::SC_SpinBoxDown;
            const QRectF button = style()->subControlRect(QStyle::CC_SpinBox, &option, control, this);
            const bool enabled = isEnabled() && (option.stepEnabled & (up ? StepUpEnabled : StepDownEnabled));
            painter.setPen(QPen(enabled ? theme.textSecondary : theme.textDisabled, qMax(1., scale_), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const QPointF center = button.center();
            const double halfWidth = 3*scale_, halfHeight = 1.5*scale_, direction = up ? 1. : -1.;
            QPainterPath path;
            path.moveTo(center + QPointF(-halfWidth, direction*halfHeight));
            path.lineTo(center + QPointF(0, -direction*halfHeight));
            path.lineTo(center + QPointF(halfWidth, direction*halfHeight));
            painter.drawPath(path);
        }
    }
private:
    double scale_ = 1;
    bool fixedWidth_ = false;
};
} // namespace ImageBrowser
