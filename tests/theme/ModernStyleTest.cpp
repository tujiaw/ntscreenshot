#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionButton>

#include "core/theme/ModernStyle.h"

namespace {

QString loadTestStyleSheet()
{
    QFile file(QStringLiteral(":/darkstyle/modernstyle.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QString style = QString::fromUtf8(file.readAll());
    style.replace(QStringLiteral("@SURFACE_SUBTLE@"), QStringLiteral("#F7F8FA"));
    style.replace(QStringLiteral("@SUCCESS@"), QStringLiteral("#16A36A"));
    style.replace(QStringLiteral("@DANGER@"), QStringLiteral("#DC3545"));
    style.replace(QStringLiteral("@TEXT_DISABLED@"), QStringLiteral("#A5ADBA"));
    style.replace(QStringLiteral("@BORDER@"), QStringLiteral("#E1E5EA"));
    style.replace(QRegularExpression(QStringLiteral("@[A-Z_]+@")), QStringLiteral("#2F7EF7"));
    return style;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle(new ModernStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
    QPalette palette = app.palette();
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2F7EF7")));
    app.setPalette(palette);
    app.setStyleSheet(loadTestStyleSheet());

    QRadioButton radio(QStringLiteral("RGB"));
    radio.setChecked(true);
    radio.resize(100, 40);
    radio.ensurePolished();

    QImage rendered(radio.size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::white);
    QPainter painter(&rendered);
    radio.render(&painter);
    painter.end();

    QStyleOptionButton option;
    option.initFrom(&radio);
    option.rect = radio.rect();
    option.state |= QStyle::State_On;
    const QRect indicator = radio.style()->subElementRect(
        QStyle::SE_RadioButtonIndicator, &option, &radio);
    QCheckBox checkbox(QStringLiteral("Auto start"));
    checkbox.ensurePolished();
    QStyleOptionButton checkboxOption;
    checkboxOption.initFrom(&checkbox);
    const QRect checkboxIndicator = checkbox.style()->subElementRect(
        QStyle::SE_CheckBoxIndicator, &checkboxOption, &checkbox);
    int centerWhitePixels = 0;
    const QRect center = QRect(QPoint(), QSize(4, 4));
    const QPoint indicatorCenter = QRectF(indicator).center().toPoint();
    const QRect centeredDot = center.translated(indicatorCenter - center.center());
    for (int y = centeredDot.top(); y <= centeredDot.bottom(); ++y) {
        for (int x = centeredDot.left(); x <= centeredDot.right(); ++x) {
            const QColor pixel = rendered.pixelColor(x, y);
            if (pixel.red() > 230 && pixel.green() > 230 && pixel.blue() > 230) {
                ++centerWhitePixels;
            }
        }
    }

    int accentFillPixels = 0;
    const int fillRadius = indicator.width() / 3;
    const QPoint fillSamples[] = {
        indicatorCenter + QPoint(fillRadius, 0),
        indicatorCenter + QPoint(-fillRadius, 0),
        indicatorCenter + QPoint(0, fillRadius),
        indicatorCenter + QPoint(0, -fillRadius)
    };
    for (const QPoint& sample : fillSamples) {
        const QColor pixel = rendered.pixelColor(sample);
        if (pixel.blue() > 180 && pixel.red() < 100 && pixel.green() < 160) {
            ++accentFillPixels;
        }
    }

    int horizontalAsymmetry = 0;
    int verticalAsymmetry = 0;
    for (int offset = 0; offset < indicator.width() / 2; ++offset) {
        int leftAccentPixels = 0;
        int rightAccentPixels = 0;
        for (int y = indicator.top(); y <= indicator.bottom(); ++y) {
            const QColor left = rendered.pixelColor(indicator.left() + offset, y);
            const QColor right = rendered.pixelColor(indicator.right() - offset, y);
            leftAccentPixels += left.blue() > 180 && left.red() < 100 && left.green() < 160;
            rightAccentPixels += right.blue() > 180 && right.red() < 100 && right.green() < 160;
        }
        horizontalAsymmetry += qAbs(leftAccentPixels - rightAccentPixels);
    }
    for (int offset = 0; offset < indicator.height() / 2; ++offset) {
        int topAccentPixels = 0;
        int bottomAccentPixels = 0;
        for (int x = indicator.left(); x <= indicator.right(); ++x) {
            const QColor top = rendered.pixelColor(x, indicator.top() + offset);
            const QColor bottom = rendered.pixelColor(x, indicator.bottom() - offset);
            topAccentPixels += top.blue() > 180 && top.red() < 100 && top.green() < 160;
            bottomAccentPixels += bottom.blue() > 180 && bottom.red() < 100 && bottom.green() < 160;
        }
        verticalAsymmetry += qAbs(topAccentPixels - bottomAccentPixels);
    }

    if (indicator.size() != checkboxIndicator.size()
        || centerWhitePixels < 12 || accentFillPixels != 4) {
        qCritical("A checked radio indicator must match checkbox size and use accent fill with a white center dot");
        return 1;
    }
    if (horizontalAsymmetry != 0 || verticalAsymmetry != 0) {
        qCritical("A radio indicator must be centered inside its paint rectangle without clipped edges");
        return 1;
    }

    QFrame configPanel;
    configPanel.setObjectName(QStringLiteral("httpServerConfigPanel"));
    configPanel.resize(120, 60);
    configPanel.ensurePolished();
    QImage configPanelImage(configPanel.size(), QImage::Format_ARGB32_Premultiplied);
    configPanelImage.fill(Qt::white);
    QPainter configPainter(&configPanelImage);
    configPanel.render(&configPainter);
    configPainter.end();
    if (configPanelImage.pixelColor(configPanel.rect().center())
        != QColor(QStringLiteral("#F7F8FA"))) {
        qCritical("The HTTP server configuration panel must be visually separated from the page");
        return 1;
    }

    QLabel statusDot;
    statusDot.setObjectName(QStringLiteral("httpServerStatusDot"));
    statusDot.setProperty("status", QStringLiteral("running"));
    statusDot.resize(8, 8);
    statusDot.ensurePolished();
    QImage statusDotImage(statusDot.size(), QImage::Format_ARGB32_Premultiplied);
    statusDotImage.fill(Qt::white);
    QPainter statusPainter(&statusDotImage);
    statusDot.render(&statusPainter);
    statusPainter.end();
    if (statusDotImage.pixelColor(statusDot.rect().center())
        != QColor(QStringLiteral("#16A36A"))) {
        qCritical("A running HTTP server must have a visible success status indicator");
        return 1;
    }

    QPushButton disabledStopButton(QStringLiteral("Stop"));
    disabledStopButton.setProperty("uiRole", QStringLiteral("danger"));
    disabledStopButton.setEnabled(false);
    disabledStopButton.resize(80, 34);
    disabledStopButton.ensurePolished();
    QImage stopButtonImage(disabledStopButton.size(), QImage::Format_ARGB32_Premultiplied);
    stopButtonImage.fill(Qt::white);
    QPainter stopButtonPainter(&stopButtonImage);
    disabledStopButton.render(&stopButtonPainter);
    stopButtonPainter.end();
    if (stopButtonImage.pixelColor(disabledStopButton.rect().center())
        != QColor(QStringLiteral("#F7F8FA"))) {
        qCritical("A disabled danger action must not look active");
        return 1;
    }
    return 0;
}
