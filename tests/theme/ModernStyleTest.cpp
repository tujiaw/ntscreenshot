#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QImage>
#include <QPainter>
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
    int centerAccentPixels = 0;
    const QRect center = QRect(QPoint(), QSize(8, 8));
    const QRect centeredDot = center.translated(indicator.center() - center.center());
    for (int y = centeredDot.top(); y <= centeredDot.bottom(); ++y) {
        for (int x = centeredDot.left(); x <= centeredDot.right(); ++x) {
            const QColor pixel = rendered.pixelColor(x, y);
            if (pixel.blue() > 180 && pixel.red() < 100 && pixel.green() < 160) {
                ++centerAccentPixels;
            }
        }
    }

    if (indicator.size() != checkboxIndicator.size() || centerAccentPixels < 40) {
        qCritical("A checked radio indicator must match checkbox size and have a solid center dot");
        return 1;
    }
    return 0;
}
