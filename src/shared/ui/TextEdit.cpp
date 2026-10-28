#include "TextEdit.h"
#include <QTextDocument>
#include <QKeyEvent>
#include <QTextCursor>
#include <QtMath>

const int PADDING = 6;
const int BASE_WIDTH = 50;
const int BASE_HEIGHT = 26;
TextEdit::TextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setAcceptDrops(true);
    this->setAcceptRichText(false);
    connect(this, &QTextEdit::textChanged, this, &TextEdit::onTextChanged);
    this->setFixedSize(BASE_WIDTH, BASE_HEIGHT);
    this->setLineWrapMode(QTextEdit::NoWrap);
    this->document()->setDocumentMargin(PADDING);
}

void TextEdit::onTextChanged()
{
    // Document dimensions already include the same padding used to lay out
    // the caret and text, including an empty line and mixed-font input.
    const int w = qCeil(document()->idealWidth()) + 2 * frameWidth() + cursorWidth();
    const int h = qCeil(document()->size().height()) + 2 * frameWidth();
    this->setFixedWidth(qMax(w, BASE_WIDTH));
    this->setFixedHeight(qMax(h, BASE_HEIGHT));
}

QPoint TextEdit::startCursorPoint()
{
    const QTextCursor first(document());
    return viewport()->mapTo(parentWidget(), cursorRect(first).topLeft());
}

void TextEdit::setStyle(const QFont &font, const QColor &color)
{
    const QString fontSize = font.pixelSize() > 0
        ? QStringLiteral("%1px").arg(font.pixelSize())
        : QStringLiteral("%1pt").arg(font.pointSizeF());

    this->setStyleSheet(QStringLiteral(
        "background:transparent;"
        "color:%1;"
        "border:1px dotted %2;"
        "padding:0px;"
        "font-size:%3;")
        .arg(color.name(), color.name(), fontSize));
    this->setFont(font);
    this->document()->setDefaultFont(font);
    this->setCurrentFont(font);
    this->ensurePolished();
    onTextChanged();
}

void TextEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        emit cancelRequested();
        e->accept();
        return;
    }
    if ((e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
        && e->modifiers().testFlag(Qt::ControlModifier)) {
        emit commitRequested();
        e->accept();
        return;
    }
    QTextEdit::keyPressEvent(e);
}
