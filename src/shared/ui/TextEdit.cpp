#include "TextEdit.h"
#include <QTextDocument>
#include <QKeyEvent>

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
    this->document()->setDocumentMargin(0);
}

void TextEdit::onTextChanged()
{
    QFontMetrics fm = this->fontMetrics();
    QStringList strList = this->toPlainText().split("\n");
    int maxTextWidth = 0;
    int maxTextHeight = 0;
    for (const QString &line : strList) {
        int w = fm.horizontalAdvance(line);
        if (w > maxTextWidth) {
            maxTextWidth = w;
        }
        maxTextHeight += fm.height() + 2;
    }

    int w = PADDING * 2 + maxTextWidth;
    int h = PADDING * 2 + maxTextHeight;
    this->setFixedWidth(qMax(w, BASE_WIDTH));
    this->setFixedHeight(qMax(h, BASE_HEIGHT));
}

QPoint TextEdit::startCursorPoint()
{
    return viewport()->mapTo(parentWidget(), QPoint(0, 0));
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
