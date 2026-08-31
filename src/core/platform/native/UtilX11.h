#pragma once

#include <QRect>
#include <QtGui/qwindowdefs.h>

namespace UtilX11 {
    void cacheAllWindows();
    bool getRectFromCurrentPoint(WId filterSelf, QRect &out_rect);
    bool getSmallestWindowFromCursor(QRect &smallestRect);
}
