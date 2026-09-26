#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QWidget>
#include <cstdio>

#include "core/theme/ThemeManager.h"
#include "modules/capture/annotation/DrawSettings.h"

namespace {
double testScale = 1.0;
}

namespace Util {
int scaleSize(int size)
{
    return static_cast<int>(size * testScale);
}

void scaleLayoutMargins(QLayout *layout, int left, int top, int right, int bottom)
{
    layout->setContentsMargins(scaleSize(left), scaleSize(top),
                               scaleSize(right), scaleSize(bottom));
}
}

const ThemeTokens& ThemeManager::tokens()
{
    static const ThemeTokens tokens = [] {
        ThemeTokens value;
        value.surface = Qt::white;
        value.surfaceRaised = Qt::white;
        value.surfaceSubtle = QColor(240, 240, 240);
        value.textSecondary = Qt::black;
        value.accent = QColor(45, 110, 240);
        return value;
    }();
    return tokens;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFile stylesheet(QStringLiteral(DRAW_SETTINGS_QSS_PATH));
    if (!stylesheet.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "Could not load application stylesheet\n");
        return 1;
    }
    QString css = QString::fromUtf8(stylesheet.readAll());
    css.replace(QRegularExpression(QStringLiteral("@[A-Z_]+@")), QStringLiteral("#777777"));
    app.setStyleSheet(css);

    QWidget parent;
    parent.resize(1200, 300);
    parent.show();
    DrawSettings settings(&parent);
    settings.show();

    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
        testScale = scale;
        settings.rescaleForDpi();
        for (int parentY : {0, 700}) {
            parent.move(0, parentY);
            app.processEvents();
            if (settings.size() != QSize(Util::scaleSize(400), Util::scaleSize(44))) {
                std::fprintf(stderr, "Panel size changed at scale %.2f\n", scale);
                return 1;
            }
            const auto buttons = settings.findChildren<QPushButton*>();
            if (buttons.size() != 20) {
                std::fprintf(stderr, "Unexpected button count: %lld\n", static_cast<long long>(buttons.size()));
                return 1;
            }
            for (const QPushButton *button : buttons) {
                const QRect bounds(button->mapTo(&settings, QPoint()), button->size());
                if (!button->isVisible() || !settings.rect().contains(bounds)) {
                    std::fprintf(stderr, "Clipped button at scale %.2f: x=%d width=%d panel=%d\n",
                                 scale, bounds.x(), bounds.width(), settings.width());
                    return 1;
                }
            }
            const auto *fontSize = settings.findChild<QComboBox*>();
            const QRect fontBounds(fontSize->mapTo(&settings, QPoint()), fontSize->size());
            if (!settings.rect().contains(fontBounds)) {
                std::fprintf(stderr, "Clipped font size control at scale %.2f\n", scale);
                return 1;
            }
        }
    }
    return 0;
}
