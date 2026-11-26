#pragma once

#include "core/imaging/ImageLimits.h"
#include <QtGlobal>

// Capture accounting is independent of matching: refused frames never advance
// the reference position or consume output coverage.
class LongScreenshotProgress {
public:
    enum class Limit { None, Height, Pixels, Memory, Frames };
    static constexpr int MaxHeight = 60000;
    static constexpr qint64 MemoryBudget = 768LL * 1024 * 1024;

    void rejected(bool motion) { if (motion && unmatched_ < 3) ++unmatched_; }
    bool needsRecovery() const { return unmatched_ >= 3; }
    qint64 height() const { return bottom_ - top_; }

    bool canCapture(qint64 frameBytes, qint64 pendingBytes = 0) const
    {
        // Reserve a new grab and the in-flight worker, plus three output-sized
        // buffers for stitching, pixmap conversion and clipboard/pin handoff.
        return retainedBytes_ + pendingBytes + 2 * frameBytes + outputBytes_ * 3 <= MemoryBudget;
    }

    Limit append(int width, int frameHeight, int shift, qint64 frameBytes,
                 qint64 transientBytes = 0)
    {
        const qint64 position = position_ + shift;
        const qint64 top = qMin(top_, position);
        const qint64 bottom = qMax(bottom_, position + frameHeight);
        const qint64 pixels = qint64(width) * (bottom - top);
        if (bottom - top > MaxHeight) return Limit::Height;
        if (pixels > ImageLimits::MaxEditablePixels) return Limit::Pixels;
        if (retainedBytes_ + frameBytes + transientBytes + pixels * 4 * 3 > MemoryBudget)
            return Limit::Memory;
        if (count_ >= 220) return Limit::Frames;
        position_ = position;
        top_ = top;
        bottom_ = bottom;
        outputBytes_ = pixels * 4;
        retainedBytes_ += frameBytes;
        ++count_;
        unmatched_ = 0;
        return Limit::None;
    }

private:
    int unmatched_ = 0;
    int count_ = 0;
    qint64 position_ = 0;
    qint64 top_ = 0;
    qint64 bottom_ = 0;
    qint64 retainedBytes_ = 0;
    qint64 outputBytes_ = 0;
};
