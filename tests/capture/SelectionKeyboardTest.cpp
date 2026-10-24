#include "modules/capture/screenshot/SelectionKeyboard.h"
#include <QDebug>

int main()
{
    const QRect bounds(0, 0, 100, 80);
    const QRect original(20, 20, 30, 25);
    struct Case { int key; QRect expanded; QRect contracted; QPoint movement; };
    const Case cases[] = {
        {Qt::Key_Left, {19, 20, 31, 25}, {21, 20, 29, 25}, {-1, 0}},
        {Qt::Key_Right, {20, 20, 31, 25}, {20, 20, 29, 25}, {1, 0}},
        {Qt::Key_Up, {20, 19, 30, 26}, {20, 21, 30, 24}, {0, -1}},
        {Qt::Key_Down, {20, 20, 30, 26}, {20, 20, 30, 24}, {0, 1}},
    };
    bool okay = true;
    auto check = [&](QRect actual, QRect expected) {
        if (actual != expected) {
            qCritical() << "Selection mismatch:" << actual << "expected" << expected;
            okay = false;
        }
    };
    for (const auto& c : cases) {
        check(selectionAfterArrow(original, bounds, c.key, Qt::ControlModifier), c.expanded);
        check(selectionAfterArrow(original, bounds, c.key, Qt::ShiftModifier), c.contracted);
        check(selectionAfterArrow(original, bounds, c.key, Qt::NoModifier), original.translated(c.movement));
        check(selectionAfterArrow(c.expanded, bounds, c.key, Qt::ShiftModifier), original);
        check(selectionAfterArrow(c.contracted, bounds, c.key, Qt::ControlModifier), original);
        check(selectionAfterArrow(original, bounds, c.key, Qt::ControlModifier | Qt::ShiftModifier), original);
        check(selectionAfterArrow(original, bounds, c.key, Qt::AltModifier), original);
        check(selectionAfterArrow(bounds, bounds, c.key, Qt::ControlModifier), bounds);
        check(selectionAfterArrow(bounds, bounds, c.key, Qt::NoModifier), bounds);
        const QRect pixel(20, 20, 1, 1);
        check(selectionAfterArrow(pixel, bounds, c.key, Qt::ShiftModifier), pixel);
    }
    // Repeated key presses stop at the edge instead of wrapping or inverting.
    QRect selection = original;
    for (int i = 0; i < 100; ++i)
        selection = selectionAfterArrow(selection, bounds, Qt::Key_Left, Qt::ControlModifier);
    check(selection, QRect(0, 20, 50, 25));
    for (int i = 0; i < 100; ++i)
        selection = selectionAfterArrow(selection, bounds, Qt::Key_Left, Qt::ShiftModifier);
    check(selection, QRect(49, 20, 1, 25));
    return okay ? 0 : 1;
}
