#include "UtilX11.h"

#include "core/platform/Util.h"

#include <QCursor>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

#include <X11/Xlib.h>

namespace UtilX11 {

static Display *display()
{
    QGuiApplication *gui = qGuiApp;
    if (!gui) {
        return nullptr;
    }
    if (auto *x11 = gui->nativeInterface<QNativeInterface::QX11Application>()) {
        return x11->display();
    }
    return nullptr;
}

static bool absGeometry(Display *dpy, Window w, QRect &out)
{
    if (!dpy || !w) {
        return false;
    }
    Window root = DefaultRootWindow(dpy);
    Window junk;
    int absX = 0;
    int absY = 0;
    unsigned int bw = 0;
    unsigned int depth = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    int x = 0;
    int y = 0;
    XGetGeometry(dpy, w, &junk, &x, &y, &width, &height, &bw, &depth);
    if (!XTranslateCoordinates(dpy, w, root, 0, 0, &absX, &absY, &junk)) {
        return false;
    }
    out.setRect(absX, absY, static_cast<int>(width), static_cast<int>(height));
    return out.isValid() && width > 0 && height > 0;
}

static Window pickTopLevelUnderCursor(Display *dpy, WId filterSelf)
{
    if (!dpy) {
        return None;
    }

    const QPoint gp = QCursor::pos();
    const int abs_x = gp.x();
    const int abs_y = gp.y();

    const Window rootWin = DefaultRootWindow(dpy);
    Window rootRet = 0;
    Window parentRet = 0;
    Window *children = nullptr;
    unsigned int n = 0;
    if (!XQueryTree(dpy, rootWin, &rootRet, &parentRet, &children, &n)) {
        return None;
    }

    Window chosen = None;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        Window w = children[i];
        if (filterSelf && static_cast<WId>(w) == filterSelf) {
            continue;
        }
        XWindowAttributes a;
        if (XGetWindowAttributes(dpy, w, &a) == 0) {
            continue;
        }
        if (a.map_state != IsViewable) {
            continue;
        }
        QRect r;
        if (!absGeometry(dpy, w, r)) {
            continue;
        }
        if (r.contains(abs_x, abs_y)) {
            chosen = w;
            break;
        }
    }
    if (children) {
        XFree(children);
    }
    return chosen;
}

void cacheAllWindows()
{
}

bool getRectFromCurrentPoint(WId filterSelf, QRect &out_rect)
{
    Display *dpy = display();
    if (!dpy) {
        return false;
    }
    const Window chosen = pickTopLevelUnderCursor(dpy, filterSelf);
    if (chosen == None) {
        return false;
    }
    return absGeometry(dpy, chosen, out_rect);
}

bool getSmallestWindowFromCursor(QRect &smallestRect)
{
    Display *dpy = display();
    if (!dpy) {
        return false;
    }
    const Window chosen = pickTopLevelUnderCursor(dpy, 0);
    if (chosen == None) {
        return false;
    }
    Util::setCurrentHwnd(static_cast<WId>(chosen));
    return absGeometry(dpy, chosen, smallestRect);
}

} // namespace UtilX11
