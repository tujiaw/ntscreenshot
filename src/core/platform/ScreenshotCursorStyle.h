#pragma once

// Stable values stored in settings; keep them independent of menu order.
enum class ScreenshotCursorStyle {
    Color = 0,
    Classic = 1,
    Contrast = 2,
    System = 3
};

inline ScreenshotCursorStyle validatedCursorStyle(int value)
{
    return value >= 0 && value <= 3 ? static_cast<ScreenshotCursorStyle>(value)
                                  : ScreenshotCursorStyle::Color;
}
