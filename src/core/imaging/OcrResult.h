#pragma once

#include <QByteArray>
#include <QPolygonF>
#include <QStringList>
#include <QVector>

struct OcrSpan {
    QPolygonF polygon;
    int line = 0;
    int start = 0;
    int length = 0;
};

struct OcrResult {
    QStringList lines;
    QVector<OcrSpan> spans;
    QString selectedText(int first, int last) const;
    static OcrResult parse(const QByteArray& content);
};
