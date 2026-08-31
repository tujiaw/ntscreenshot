#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

struct CodeScanResult {
    QString text;
    QString type; // "QR" / barcode type / "Unknown"
    QRect boundingRect;
};

namespace CodeScanner {

// Detect and decode QR codes and barcodes in the image.
QVector<CodeScanResult> scan(const QImage& image);

// Convenience: join all decoded texts with newlines; empty if none found.
QString scanToText(const QImage& image);

} // namespace CodeScanner
