#include "shared/ui/TextEdit.h"

#include <QApplication>
#include <QDebug>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QWidget parent;
    parent.resize(1200, 1000);
    TextEdit editor(&parent);
    editor.move(20, 30);
    parent.show();
    editor.show();
    bool okay = true;
    auto check = [&](bool condition, const char* message) {
        if (!condition) qWarning() << message << "font:" << editor.font().pixelSize()
            << "text:" << editor.toPlainText() << "caret:" << editor.cursorRect()
            << "editor:" << editor.size();
        okay &= condition;
    };
    for (int size : {8, 16, 32, 64}) {
        editor.clear();
        QFont font = app.font();
        font.setPixelSize(size);
        editor.setStyle(font, Qt::red);
        for (const QString& text : {QString(), QStringLiteral("文字 Abg"),
                                    QStringLiteral("第一行 Abg\n第二行 text\nThird line")}) {
            editor.setPlainText(text);
            editor.setFocus();
            app.processEvents();
            QTextCursor first(editor.document());
            QTextCursor last(editor.document());
            last.movePosition(QTextCursor::End);
            const QRect firstCaret = editor.cursorRect(first);
            const QRect lastCaret = editor.cursorRect(last);
            const int top = editor.viewport()->mapTo(&editor, firstCaret.topLeft()).y();
            const int bottom = editor.height() - 1
                - editor.viewport()->mapTo(&editor, lastCaret.bottomLeft()).y();
            check(top >= 5 && qAbs(top - bottom) <= 2, "text block has balanced vertical padding");
            check(editor.horizontalScrollBar()->maximum() == 0
                && editor.verticalScrollBar()->maximum() == 0, "text fits without clipping or scrolling");
            editor.setTextCursor(last);
            check(editor.startCursorPoint() == editor.viewport()->mapTo(&parent, firstCaret.topLeft()),
                  "committed text starts at the first line regardless of caret position");
        }
        editor.clear();
        app.processEvents();
        const QPoint caretCenter = editor.viewport()->mapTo(&editor, editor.cursorRect().center());
        check(qAbs(caretCenter.y() - editor.rect().center().y()) <= 1,
              "empty caret is vertically centered after reuse");
    }
    return okay ? 0 : 1;
}
