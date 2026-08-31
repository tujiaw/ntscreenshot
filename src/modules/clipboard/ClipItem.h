#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

// Mirrors the portable clipboard item model used by wtl_clipboard,
// adapted to Qt value types.

enum class ClipKind {
    Text,
    Image
};

struct ClipItem {
    ClipKind kind = ClipKind::Text;
    QDateTime capturedAt = QDateTime::currentDateTime();
    QString text;
    QByteArray data;   // PNG bytes for image items
    int width = 0;
    int height = 0;
    quint64 pixelHash = 0;   // FNV-1a hash of decoded pixels (0 = unknown)
};
