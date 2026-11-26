#include "modules/capture/long_screenshot/LongScreenshotProgress.h"
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Failed at line %d\n", __LINE__); return 1; } } while (false)

int main()
{
    LongScreenshotProgress progress;
    progress.rejected(true);
    progress.rejected(false); // A stationary sample must not hide an unresolved failure.
    CHECK(!progress.needsRecovery());
    progress.rejected(true);
    progress.rejected(true);
    CHECK(progress.needsRecovery());
    CHECK(progress.append(120, 500, 0, 240000) == LongScreenshotProgress::Limit::None);
    CHECK(!progress.needsRecovery());
    CHECK(progress.append(120, 500, -300, 240000) == LongScreenshotProgress::Limit::None);
    CHECK(progress.height() == 800);
    CHECK(progress.append(120, 500, 300, 240000) == LongScreenshotProgress::Limit::None);
    CHECK(progress.height() == 800); // Revisiting content must not consume output height.
    for (int i = 0; i < 118; ++i)
        CHECK(progress.append(120, 500, 499, 240000) == LongScreenshotProgress::Limit::None);
    CHECK(progress.height() == 59682);
    CHECK(progress.append(120, 500, 499, 240000) == LongScreenshotProgress::Limit::Height);
    CHECK(progress.height() == 59682); // Refusal preserves the last complete result.

    LongScreenshotProgress exactHeight;
    CHECK(exactHeight.append(120, 59999, 0, 28799520) == LongScreenshotProgress::Limit::None);
    CHECK(exactHeight.append(120, 59999, 1, 28799520) == LongScreenshotProgress::Limit::None);
    CHECK(exactHeight.height() == 60000);
    CHECK(exactHeight.append(120, 59999, 1, 28799520) == LongScreenshotProgress::Limit::Height);

    LongScreenshotProgress pixels;
    CHECK(pixels.append(72000, 500, 0, 144000000) == LongScreenshotProgress::Limit::None);
    CHECK(pixels.append(72000, 500, 250, 144000000) == LongScreenshotProgress::Limit::Memory);
    LongScreenshotProgress tooWide;
    CHECK(tooWide.append(72001, 1000, 0, 288004000) == LongScreenshotProgress::Limit::Pixels);
    LongScreenshotProgress frames;
    for (int i = 0; i < 220; ++i)
        CHECK(frames.append(120, 500, 0, 240000) == LongScreenshotProgress::Limit::None);
    CHECK(frames.append(120, 500, 0, 240000) == LongScreenshotProgress::Limit::Frames);
    LongScreenshotProgress memory;
    CHECK(!memory.canCapture(600LL * 1024 * 1024));
    CHECK(memory.canCapture(240000));
    CHECK(!memory.canCapture(240000, 800LL * 1024 * 1024));
    return 0;
}
