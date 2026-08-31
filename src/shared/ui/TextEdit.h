#pragma once

#include <QTextEdit>

class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    TextEdit(QWidget *parent);
    QPoint startCursorPoint();
    void setStyle(const QColor &color);

public slots:
    void onTextChanged();

protected:
    virtual void keyPressEvent(QKeyEvent *e);
};
