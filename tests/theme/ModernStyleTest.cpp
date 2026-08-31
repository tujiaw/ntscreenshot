#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionButton>

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
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
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
    int accentPixels = 0;
    for (int y = indicator.top(); y <= indicator.bottom(); ++y) {
        for (int x = indicator.left(); x <= indicator.right(); ++x) {
            const QColor pixel = rendered.pixelColor(x, y);
            if (pixel.blue() > 180 && pixel.red() < 100 && pixel.green() < 160) {
                ++accentPixels;
            }
        }
    }

    const int indicatorArea = indicator.width() * indicator.height();
    if (indicatorArea <= 0 || accentPixels * 2 <= indicatorArea) {
        qCritical("A checked radio indicator must have a clearly visible accent fill");
        return 1;
    }
    return 0;
}
