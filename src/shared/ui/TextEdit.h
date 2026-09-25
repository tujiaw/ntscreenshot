#pragma once

#include <QTextEdit>

class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    TextEdit(QWidget *parent);
    QPoint startCursorPoint();
    void setStyle(const QFont &font, const QColor &color);

signals:
    void commitRequested();
    void cancelRequested();

public slots:
    void onTextChanged();

protected:
    void keyPressEvent(QKeyEvent *e) override;
};
