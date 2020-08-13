#include "TextEdit.h"
#include <QDebug>
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
}

void TextEdit::onTextChanged()
{
    QFontMetrics fm = this->fontMetrics();
    QStringList strList = this->toPlainText().split("\n");
    int maxTextWidth = 0;
    int maxTextHeight = 0;
    foreach(const QString &line, strList) {
        int w = fm.width(line);
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
    return this->pos() + QPoint(5, 5);
}

void TextEdit::setStyle(const QColor &color)
{
    this->setStyleSheet(QString("background:transparent;color:%1;border:1px dotted %2;").arg(color.name()).arg(color.name()));
}

void TextEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Enter | e->key() == Qt::Key_Return) {

    }
    QTextEdit::keyPressEvent(e);
}