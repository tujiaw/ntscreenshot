#pragma once

#include <QProxyStyle>

class ModernStyle final : public QProxyStyle
{
public:
    explicit ModernStyle(QStyle* baseStyle = nullptr, int radioIndicatorSize = 20);

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override;

private:
    int radioIndicatorSize_;
};
