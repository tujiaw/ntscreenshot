#include "Util.h"
#include "CaptureCursor.h"
#include <QPixmapCache>
#include <QStringList>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QWidget>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QFont>
#include <QLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QKeyEvent>
#include <QKeySequence>
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QStandardPaths>
#include <QTimer>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QProcess>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDesktopServices>
#include <QFileInfo>
#ifdef Q_OS_WIN
#pragma warning(disable:4091)
#include <ShlObj.h>
#include <ShellScalingApi.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shcore.lib")
#include "HWndRectCache.h"
#elif defined(Q_OS_LINUX)
#include "core/platform/native/UtilX11.h"
#endif

/*
* Removed unused EnumerateFileInDirectory
*/

namespace Util
{
#ifdef Q_OS_WIN
    namespace {
    HWndRectCacheManager windowRectCache;
    }
#endif
    QRect desktopRect()
    {
#ifdef Q_OS_WIN
        int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        return QRect(x, y, w, h);
#else
        QRect rect;
        for (QScreen *screen : QGuiApplication::screens()) {
            double dpr = screen->devicePixelRatio();
            QRect logical = screen->geometry();
            rect = rect.united(QRect(logical.x() * dpr, logical.y() * dpr, logical.width() * dpr, logical.height() * dpr));
        }
        return rect;
#endif
    }

    QRect desktopLocalRect()
    {
        const QRect desktop = desktopRect();
        return QRect(QPoint(0, 0), desktop.size());
    }

    QPoint toDesktopLocal(const QPoint &physicalPoint)
    {
        return physicalPoint - desktopRect().topLeft();
    }

    QRect toDesktopLocal(const QRect &physicalRect)
    {
        return QRect(toDesktopLocal(physicalRect.topLeft()), physicalRect.size());
    }

    QRect clampToDesktopLocal(const QRect &rect)
    {
        return rect.normalized().intersected(desktopLocalRect());
    }

    QImage grabDesktopImage(const QRect &desktopLocalRect)
    {
        QRect localRect = desktopLocalRect.isNull() ? Util::desktopLocalRect() : clampToDesktopLocal(desktopLocalRect);
        if (localRect.isEmpty()) {
            return QImage();
        }

#ifdef Q_OS_WIN
        const QRect desktop = Util::desktopRect();
        HDC hScreenDC = CreateDC(L"DISPLAY", NULL, NULL, NULL);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, localRect.width(), localRect.height());
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

        const QPoint physicalTopLeft = desktop.topLeft() + localRect.topLeft();
        BitBlt(hMemoryDC, 0, 0, localRect.width(), localRect.height(),
               hScreenDC, physicalTopLeft.x(), physicalTopLeft.y(), SRCCOPY | CAPTUREBLT);

        SelectObject(hMemoryDC, hOldBitmap);

        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof(BITMAPINFOHEADER));
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = -bmp.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_RGB32);
        GetDIBits(hScreenDC, hBitmap, 0, bmp.bmHeight, image.bits(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        DeleteDC(hScreenDC);
        return image;
#else
        QImage image(localRect.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        for (QScreen *screen : QGuiApplication::screens()) {
            const qreal dpr = screen->devicePixelRatio();
            const QRect logical = screen->geometry();
            const QRect screenPhysical(logical.x() * dpr, logical.y() * dpr,
                                       logical.width() * dpr, logical.height() * dpr);
            const QRect screenLocal = Util::toDesktopLocal(screenPhysical);
            const QRect intersection = screenLocal.intersected(localRect);
            if (intersection.isEmpty()) {
                continue;
            }

            QPixmap screenPixmap = screen->grabWindow(0);
            const QRect sourceRect(intersection.topLeft() - screenLocal.topLeft(), intersection.size());
            const QPoint targetPoint = intersection.topLeft() - localRect.topLeft();
            painter.drawPixmap(targetPoint, screenPixmap, sourceRect);
        }
        return image;
#endif
    }

	QStringList getFiles(const QString &path, bool containsSubDir)
	{
		QStringList result;
        QDir::Filters filters = QDir::Files;
        QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
        
        if (containsSubDir) {
            flags |= QDirIterator::Subdirectories;
        }

		QDirIterator it(path, QStringList(), filters, flags);
		while (it.hasNext()) {
			result.push_back(it.next());
		}

		return result;
	}


	bool shellExecute(const QString &path)
	{
        return shellExecute(path, "open");
	}

    // tostring/towstring removed in favor of native QString conversions

    bool shellExecute(const QString &path, const QString &operation)
    {
        if (path.isEmpty()) {
            return false;
        }

#ifdef Q_OS_WIN
        HINSTANCE hinst = ShellExecute(nullptr, operation.toStdWString().c_str(), path.toStdWString().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        LONG64 result = (LONG64)hinst;
        if (result <= 32) {
            qDebug() << "shellExecute failed, code:" << result;
            return false;
        }
        return true;
#else
        Q_UNUSED(operation);
        return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
    }

	bool locateFile(const QString &dir)
	{
        if (dir.isEmpty()) {
            return false;
        }

#ifdef Q_OS_WIN
        QString cmd = QString("/select, \"%1\"").arg(QDir::toNativeSeparators(dir));
        qDebug() << cmd;
        HINSTANCE hinst = ::ShellExecute(nullptr, L"open", L"explorer.exe", cmd.toStdWString().c_str(), nullptr, SW_SHOW);
        LONG64 result = (LONG64)hinst;
        if (result <= 32) {
            qDebug() << "locateFile failed, code:" << result;
            return false;
        }
        return true;
#else
        const QFileInfo fi(dir);
        return QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
	}

	void setForegroundWindow(QWidget *widget)
	{
		if (!widget) {
            return;
        }
#ifdef Q_OS_WIN
        ::SetForegroundWindow(reinterpret_cast<HWND>(widget->winId()));
#else
        widget->activateWindow();
        widget->raise();
#endif
	}

	void setWndTopMost(QWidget *widget)
	{
		if (!widget) {
            return;
        }
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        RECT rect;
        GetWindowRect(hwnd, &rect);
        SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, abs(rect.right - rect.left), abs(rect.bottom - rect.top), SWP_SHOWWINDOW);
#else
        widget->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        widget->show();
#endif
	}

	void cancelTopMost(QWidget *widget)
	{
		if (!widget) {
            return;
        }
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        RECT rect;
        GetWindowRect(hwnd, &rect);
        SetWindowPos(hwnd, HWND_NOTOPMOST, rect.left, rect.top, abs(rect.right - rect.left), abs(rect.bottom - rect.top), SWP_SHOWWINDOW);
#else
        widget->setWindowFlag(Qt::WindowStaysOnTopHint, false);
        widget->show();
#endif
	}

	QPixmap img(const QString &name)
	{
		QString path = ":/images/" + name;
		return QPixmap(path);
	}

	QString getRunDir()
	{
        QString dir = QApplication::applicationDirPath();
        return dir;
	}

	QString getConfigDir()
	{
		QDir configDir(getWritebaleDir() + "/config");
		if (!configDir.exists()) {
			configDir.mkpath(configDir.absolutePath());
		}
		return configDir.absolutePath();
	}

    QString getWritebaleDir()
    {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        if (!dir.exists()) {
            dir.mkpath(dir.absolutePath());
        }
        if (!dir.exists()) {
            dir = QDir(getRunDir());
        }
        return dir.absolutePath();
    }

	QString getConfigPath()
	{
		return getConfigDir() + "/base.ini";
	}

    QString getLogsDir()
    {
        QDir configDir(getWritebaleDir() + "/logs");
        if (!configDir.exists()) {
            configDir.mkdir("logs");
        }
        return configDir.absolutePath();
    }

	QVariantMap json2map(const QByteArray &val)
	{
		QJsonParseError jError;
		QJsonDocument jDoc = QJsonDocument::fromJson(val, &jError);
		if (jError.error == QJsonParseError::NoError && jDoc.isObject()) {
            return jDoc.object().toVariantMap();
		}
		return QVariantMap();
	}

	QString map2json(const QVariantMap &val)
	{
		QJsonObject jobj = QJsonObject::fromVariantMap(val);
		QJsonDocument jdoc(jobj);
		return QString(jdoc.toJson(QJsonDocument::Indented));
	}

	QVariantList json2list(const QByteArray &val)
	{
		QJsonParseError jError;
		QJsonDocument jDoc = QJsonDocument::fromJson(val, &jError);
		if (jError.error == QJsonParseError::NoError && jDoc.isArray()) {
            return jDoc.array().toVariantList();
		}
		return QVariantList();
	}

	QString list2json(const QVariantList &val)
	{
		QJsonArray jArr = QJsonArray::fromVariantList(val);
		QJsonDocument jDoc(jArr);
		return QString(jDoc.toJson(QJsonDocument::Indented));
	}

    uint toKey(const QString & str) 
    {
        QKeySequence seq(str);
        uint keyCode;

        // We should only working with a single key here
        if (seq.count() == 1) {
            keyCode = seq[0];
        } else {
            // Should be here only if a modifier key (e.g. Ctrl, Alt) is pressed.

            // Add a non-modifier key "A" to the picture because QKeySequence
            // seems to need that to acknowledge the modifier. We know that A has
            // a keyCode of 65 (or 0x41 in hex)
            seq = QKeySequence(str + "+A");
            keyCode = seq[0] - 65;
        }

        return keyCode;
    }

    QString pixmapUniqueName(const QPixmap &pixmap)
    {
        QStringList strList;
        strList << "ntscreenshot_";
        strList << md5Pixmap(pixmap);
        strList << ".png";
        return strList.join("");
    }

    QString pixmapName()
    {
        return "ntscreenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
    }

    void cacheAllWindows()
    {
#ifdef Q_OS_WIN
        windowRectCache.cacheAll();
#elif defined(Q_OS_LINUX)
        UtilX11::cacheAllWindows();
#endif
    }

    bool getRectFromCurrentPoint(WId hWndMySelf, QRect &out_rect)
    {
#ifdef Q_OS_WIN
        POINT pt;
        ::GetCursorPos(&pt);

        windowRectCache.setFilterHWnd(reinterpret_cast<HWND>(hWndMySelf));

        RECT rect = windowRectCache.getWndRect(pt, TRUE);
        out_rect = QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        return out_rect.isValid() && out_rect.width() > 0 && out_rect.height() > 0;
#elif defined(Q_OS_LINUX)
        return UtilX11::getRectFromCurrentPoint(hWndMySelf, out_rect);
#else
        Q_UNUSED(hWndMySelf);
        Q_UNUSED(out_rect);
        return false;
#endif
    }

    bool getSmallestWindowFromCursor(QRect &smallestRect)
    {
#ifdef Q_OS_WIN
        HWND hwnd;
        POINT pt;
        ::GetCursorPos(&pt);
        hwnd = ::ChildWindowFromPointEx(::GetDesktopWindow(), pt, CWP_SKIPDISABLED | CWP_SKIPINVISIBLE);
        if (hwnd != nullptr) {
            HWND temp_hwnd;
            temp_hwnd = hwnd;
            while (true) {
                ::GetCursorPos(&pt);
                ::ScreenToClient(temp_hwnd, &pt);
                temp_hwnd = ::ChildWindowFromPointEx(temp_hwnd, pt, CWP_SKIPINVISIBLE);
                if (temp_hwnd == nullptr || temp_hwnd == hwnd) {
                    break;
                }
                hwnd = temp_hwnd;
            }
            RECT r;
            ::GetWindowRect(hwnd, &r);
            setCurrentHwnd(reinterpret_cast<WId>(hwnd));
            smallestRect.setRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
            return true;
        }
        return false;
#elif defined(Q_OS_LINUX)
        return UtilX11::getSmallestWindowFromCursor(smallestRect);
#else
        return false;
#endif
    }

    namespace {
    bool isWindowVisibleOnScreens(const QRect &windowRect)
    {
        const int minVisibleWidth = qMin(scaleSize(80), windowRect.width());
        const int minVisibleHeight = qMin(scaleSize(32), windowRect.height());
        for (QScreen *screen : QGuiApplication::screens()) {
            const QRect visible = windowRect.intersected(screen->availableGeometry());
            if (visible.width() >= minVisibleWidth && visible.height() >= minVisibleHeight) {
                return true;
            }
        }
        return false;
    }

    QRect centerOnPrimaryScreen(const QSize &size, const QSize &minimumSize)
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen) {
            return QRect(QPoint(), size);
        }

        const QRect avail = screen->availableGeometry();
        QSize adjustedSize = size;
        adjustedSize.setWidth(qMax(adjustedSize.width(), minimumSize.width()));
        adjustedSize.setHeight(qMax(adjustedSize.height(), minimumSize.height()));
        adjustedSize.setWidth(qMin(adjustedSize.width(), avail.width()));
        adjustedSize.setHeight(qMin(adjustedSize.height(), avail.height()));

        int x = avail.left() + qMax(0, (avail.width() - adjustedSize.width()) / 2);
        int y = avail.top() + qMax(0, (avail.height() - adjustedSize.height()) / 2);
        if (adjustedSize.width() >= avail.width()) {
            x = avail.left();
        } else {
            x = qBound(avail.left(), x, avail.right() + 1 - adjustedSize.width());
        }
        if (adjustedSize.height() >= avail.height()) {
            y = avail.top();
        } else {
            y = qBound(avail.top(), y, avail.bottom() + 1 - adjustedSize.height());
        }
        return QRect(x, y, adjustedSize.width(), adjustedSize.height());
    }
    }

    QRect fixWindowGeometry(const QRect &geometry, const QSize &minimumSize)
    {
        const QRect desktop = desktopRect();
        if (desktop.isEmpty()) {
            return geometry.normalized();
        }

        QSize size = geometry.size();
        size.setWidth(qMax(size.width(), minimumSize.width()));
        size.setHeight(qMax(size.height(), minimumSize.height()));
        size.setWidth(qMin(size.width(), desktop.width()));
        size.setHeight(qMin(size.height(), desktop.height()));

        int x = geometry.x();
        int y = geometry.y();
        if (x + size.width() > desktop.right() + 1) {
            x = desktop.right() + 1 - size.width();
        }
        if (y + size.height() > desktop.bottom() + 1) {
            y = desktop.bottom() + 1 - size.height();
        }
        if (x < desktop.left()) {
            x = desktop.left();
        }
        if (y < desktop.top()) {
            y = desktop.top();
        }

        QRect windowRect(x, y, size.width(), size.height());
        if (!isWindowVisibleOnScreens(windowRect)) {
            return centerOnPrimaryScreen(size, minimumSize);
        }
        return windowRect;
    }

    QPoint fixPoint(const QPoint &point, const QSize &size)
    {
        return fixWindowGeometry(QRect(point, size)).topLeft();
    }

    QString strKeyEvent(QKeyEvent *keyEvent)
    {
        int keyInt = keyEvent->key();
        Qt::Key key = static_cast<Qt::Key>(keyInt);
        if (key == Qt::Key_unknown) {
            return "";
        }

        QString keyText;
        if (key == Qt::Key_Control) {
            keyText = "Ctrl";
        } else if (key == Qt::Key_Shift) {
            keyText = "Shift";
        } else if (key == Qt::ALT) {
            keyText = "Alt";
        } else if (key == Qt::META) {
            keyText = "Meta";
        } else {
            // check for a combination of user clicks 
            Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
            keyText = keyEvent->text();
            if (modifiers & Qt::ShiftModifier) { keyInt += Qt::SHIFT; }
            if (modifiers & Qt::ControlModifier) { keyInt += Qt::CTRL; }
            if (modifiers & Qt::AltModifier) { keyInt += Qt::ALT; }
            if (modifiers & Qt::MetaModifier) { keyInt += Qt::META; }
            keyText = QKeySequence(keyInt).toString(QKeySequence::NativeText);
        }

        return keyText;
    }

    QString strKeySequence(const QKeySequence &key)
    {
        return key.toString(QKeySequence::NativeText);
    }

	QByteArray pixmap2ByteArray(const QPixmap& pixmap, const char* format)
	{
		QByteArray b;
		QBuffer buffer(&b);
		buffer.open(QIODevice::WriteOnly);
		pixmap.save(&buffer, format);
		return b;
	}

    QByteArray image2ByteArray(const QImage &image, const char *format)
    {
        QByteArray b;
        QBuffer buffer(&b);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, format);
        return b;
    }

	std::string getImageFormat(const char* data, int size)
	{
		if (size < 8) {
			return "";
		}

		static const unsigned char png_sig[] = { 0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A };
        
        if (size >= 8 && memcmp(data, png_sig, 8) == 0) {
            return "png";
        }
        
        // JPEG magic: FF D8
        if (size >= 2 && (unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xD8) {
            return "jpg";
        }
        
        // BMP magic: BM (0x42 0x4D)
        if (size >= 2 && (unsigned char)data[0] == 0x42 && (unsigned char)data[1] == 0x4D) {
            return "bmp";
        }

        return "";
	}

    QString md5Pixmap(const QPixmap &pixmap)
    {
        QByteArray b = pixmap2ByteArray(pixmap);
        if (b.isEmpty()) {
            return "";
        }

        return QString::fromUtf8(QCryptographicHash::hash(b, QCryptographicHash::Md5).toHex());
    }

    QString md5Image(const QImage &image)
    {
        QByteArray b = image2ByteArray(image);
        if (b.isEmpty()) {
            return "";
        }
        
        return QString::fromUtf8(QCryptographicHash::hash(b, QCryptographicHash::Md5).toHex());
    }

    const QPixmap& multicolorCursorPixmap()
    {
        static const QPixmap pixmap = CaptureCursor::crosshair(1.0);
        return pixmap;
    }

    QCursor multicolorCursor()
    {
        const qreal scale = qMax(1.0, getScreenScaleFactor());
        const QString key = QStringLiteral("nt-selection-crosshair:%1").arg(scale, 0, 'f', 3);
        QPixmap pixmap;
        if (!QPixmapCache::find(key, &pixmap)) {
            pixmap = CaptureCursor::crosshair(scale);
            QPixmapCache::insert(key, pixmap);
        }
        return QCursor(pixmap, pixmap.width() / 2, pixmap.height() / 2);
    }
    double colorDistance(QColor e1, QColor e2)
    {
        int rmean = (e1.red() + e2.red()) / 2;
        int r = e1.red() - e2.red();
        int g = e1.green() - e2.green();
        int b = e1.blue() - e2.blue();
        return sqrt((((512 + rmean)*r*r) >> 8) + 4 * g*g + (((767 - rmean)*b*b) >> 8));
    }

    QColor colorOpposite(QColor clr)
    {
        return QColor(255 - clr.red(), 255 - clr.green(), 255 - clr.blue());
    }

    static WId s_currentHwnd = 0;
    void setCurrentHwnd(WId hwnd)
    {
        s_currentHwnd = hwnd;
    }

    WId getCurrentHwnd()
    {
        return s_currentHwnd;
    }

    void intervalHandleOnce(const std::string &name, int msTime, const std::function<void()> &func)
    {
        static QMap<std::string, QTimer*> s_intervalHandleTimers;
        QTimer *timer = s_intervalHandleTimers[name];
        if (func == nullptr) {
            if (timer) {
                timer->stop();
                timer->deleteLater();
                s_intervalHandleTimers.remove(name);
            }
            return;
        }

        if (!timer) {
            timer = new QTimer();
            s_intervalHandleTimers[name] = timer;
            timer->setSingleShot(true);
            timer->setInterval(msTime);
        }

        if (timer->interval() != msTime) {
            timer->setInterval(msTime);
            timer->stop();
        }

        if (timer->isActive()) {
            return;
        }

        timer->start();
    }

    double getScreenScaleFactor()
    {
        return getScreenScaleFactor(QCursor::pos());
    }

    double getScreenScaleFactor(const QPoint &globalPoint)
    {
#ifdef Q_OS_WIN
        POINT pt{globalPoint.x(), globalPoint.y()};
        HMONITOR monitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 96;
        UINT dpiY = 96;
        if (monitor && SUCCEEDED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX > 0) {
            return static_cast<double>(dpiX) / 96.0;
        }

        HDC dc = ::GetDC(nullptr);
        if (dc) {
            const int dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
            ::ReleaseDC(nullptr, dc);
            if (dpi > 0) {
                return static_cast<double>(dpi) / 96.0;
            }
        }
#endif
        QScreen *screen = QGuiApplication::screenAt(globalPoint);
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen) {
            return screen->devicePixelRatio();
        }
        return 1.0;
    }

    // 根据DPI缩放因子调整尺寸
    int scaleSize(int size)
    {
        double scaleFactor = getScreenScaleFactor();
        return static_cast<int>(size * scaleFactor);
    }

    // 缩放窗口/控件大小
    void scaleWidget(QWidget *widget, int baseWidth, int baseHeight)
    {
        if (!widget) {
            return;
        }

        // 明确指定了基准尺寸时，无论 DPI 是否缩放都应用（scaleSize 在 factor=1.0 时原样返回）
        if (baseWidth > 0 && baseHeight > 0) {
            widget->setFixedSize(scaleSize(baseWidth), scaleSize(baseHeight));
            return;
        }

        // 未指定基准尺寸时，仅在高 DPI 下对当前尺寸做等比放大
        double scaleFactor = getScreenScaleFactor();
        if (scaleFactor <= 1.0) {
            return;
        }

        QSize currentSize = widget->size();
        if (currentSize.width() > 0 && currentSize.height() > 0) {
            widget->resize(scaleSize(currentSize.width()), scaleSize(currentSize.height()));
        }
    }

    // 缩放字体对象
    void scaleFont(QFont &font)
    {
        scaleFont(font, getScreenScaleFactor());
    }

    void scaleFont(QFont &font, double scaleFactor)
    {
        if (scaleFactor <= 1.0) {
            return;
        }

        if (font.pointSize() > 0) {
            font.setPointSize(static_cast<int>(font.pointSize() * scaleFactor));
        } else if (font.pixelSize() > 0) {
            font.setPixelSize(static_cast<int>(font.pixelSize() * scaleFactor));
        }
    }

    // 缩放控件字体
    void scaleFont(QWidget *widget)
    {
        if (!widget) {
            return;
        }

        QFont font = widget->font();
        scaleFont(font);
        widget->setFont(font);
    }

    // 缩放布局边距
    void scaleLayoutMargins(QLayout *layout, int left, int top, int right, int bottom)
    {
        if (!layout) {
            return;
        }

        double scaleFactor = getScreenScaleFactor();
        if (scaleFactor <= 1.0) {
            layout->setContentsMargins(left, top, right, bottom);
            return;
        }

        layout->setContentsMargins(
            static_cast<int>(left * scaleFactor),
            static_cast<int>(top * scaleFactor),
            static_cast<int>(right * scaleFactor),
            static_cast<int>(bottom * scaleFactor)
        );
    }

    // 综合DPI适配方法
    void scaleWidgetDPI(QWidget *widget, int baseWidth, int baseHeight, bool scaleFont)
    {
        if (!widget) {
            return;
        }

        if (baseWidth > 0 && baseHeight > 0) {
            scaleWidget(widget, baseWidth, baseHeight);
        }

        // 字体缩放仅在高 DPI 下有意义
        double scaleFactor = getScreenScaleFactor();
        if (scaleFont && scaleFactor > 1.0) {
            Util::scaleFont(widget);
        }
    }

    UploadResult uploadToGitHub(const QByteArray& imageData, const QString& fileName, const GitHubImageBedConfig& config)
    {
        UploadResult result;
        result.success = false;

        if (config.owner.isEmpty() || config.repo.isEmpty() || config.token.isEmpty()) {
            result.message = "GitHub 图床配置不完整（用户名、仓库名、Token 均为必填）";
            return result;
        }

        QString branch     = config.branch.isEmpty() ? "main" : config.branch;
        QString pathPrefix = config.pathPrefix.isEmpty() ? "img" : config.pathPrefix;
        QString datePart   = QDateTime::currentDateTime().toString("yyyyMM");
        QString filePath   = QString("%1/%2/%3").arg(pathPrefix, datePart, fileName);

        QString apiUrl = QString("https://api.github.com/repos/%1/%2/contents/%3")
                             .arg(config.owner, config.repo, filePath);

        QJsonObject body;
        body["message"] = "upload via ntscreenshot";
        body["content"] = QString::fromLatin1(imageData.toBase64());
        body["branch"]  = branch;

        QUrl url(apiUrl);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", QString("Bearer %1").arg(config.token).toUtf8());
        req.setRawHeader("Accept", "application/vnd.github.v3+json");
        req.setRawHeader("User-Agent", "ntscreenshot");

        QNetworkAccessManager manager;
        QEventLoop loop;
        QByteArray bodyData = QJsonDocument(body).toJson();
        QNetworkReply *reply = manager.put(req, bodyData);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray response = reply->readAll();
        reply->deleteLater();

        if (statusCode == 201) {
            QString cdnBase = config.cdnUrl.isEmpty()
                ? QString("https://cdn.jsdelivr.net/gh/%1/%2@%3").arg(config.owner, config.repo, branch)
                : config.cdnUrl;
            result.success = true;
            result.fullUrl  = cdnBase + "/" + filePath;
            result.message  = result.fullUrl;
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(response);
            result.message = doc.object().value("message").toString();
            if (result.message.isEmpty()) {
                result.message = QString("上传失败，HTTP %1").arg(statusCode);
            }
        }
        return result;
    }
}
