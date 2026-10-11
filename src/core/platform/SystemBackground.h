#pragma once

#include <QImage>
#include <QString>

namespace SystemBackground {
enum class Target { Desktop, LockScreen };
struct Result {
    bool success = false;
    QString path;
    QString error;
};
bool supported();
// Worker-thread, read-only probe of the actual COM/WinRT services; does not set a background.
Result checkAvailability(Target target);
// Persistent, opaque JPEG snapshot; also allows preparation to be tested without changing OS settings.
Result prepare(const QImage& image, const QString& cacheDirectory);
// Runs on a worker thread; never blocks the UI on Windows Runtime operations.
Result apply(const QImage& image, Target target);
}
