#pragma once

#include <QRect>
#include <Qt>

// The arrow selects an edge; Ctrl expands it and Shift contracts it.
inline QRect selectionAfterArrow(const QRect& selection, const QRect& bounds,
                                int key, Qt::KeyboardModifiers modifiers)
{
    if (modifiers & ~(Qt::ControlModifier | Qt::ShiftModifier)) return selection;
    const bool expand = modifiers.testFlag(Qt::ControlModifier);
    const bool shrink = modifiers.testFlag(Qt::ShiftModifier);
    if (expand && shrink) return selection;

    QPoint direction;
    switch (key) {
    case Qt::Key_Left: direction = {-1, 0}; break;
    case Qt::Key_Right: direction = {1, 0}; break;
    case Qt::Key_Up: direction = {0, -1}; break;
    case Qt::Key_Down: direction = {0, 1}; break;
    default: return selection;
    }

    QRect result = selection;
    if (!expand && !shrink) {
        result.translate(direction);
    } else {
        const int step = shrink ? -1 : 1;
        switch (key) {
        case Qt::Key_Left: result.setLeft(result.left() - step); break;
        case Qt::Key_Right: result.setRight(result.right() + step); break;
        case Qt::Key_Up: result.setTop(result.top() - step); break;
        case Qt::Key_Down: result.setBottom(result.bottom() + step); break;
        }
    }
    return result.isValid() && bounds.contains(result) ? result : selection;
}
