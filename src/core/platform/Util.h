#pragma once

#include <QObject>
#include <QPixmap>
#include <QCursor>
#include <QRect>
#include <functional>
#include <QtGui/qwindowdefs.h>
#include "core/foundation/Constants.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

class QWidget;
class QKeyEvent;
class QKeySequence;
class QLayout;
class QFont;
namespace Util {
	QStringList getFiles(const QString &path, bool containsSubDir = true);
	bool shellExecute(const QString &path);
    bool shellExecute(const QString &path, const QString &operation);
	bool locateFile(const QString &dir);
	void setForegroundWindow(QWidget *widget);
	void setWndTopMost(QWidget *widget);
	void cancelTopMost(QWidget *widget);
	QPixmap img(const QString &name);
	QString getRunDir();
    QString getWritebaleDir();
	QString getConfigDir();
	QString getConfigPath();
    QString getLogsDir();
    QRect desktopRect();
    QRect desktopLocalRect();
    QPoint toDesktopLocal(const QPoint &physicalPoint);
    QRect toDesktopLocal(const QRect &physicalRect);
    QRect clampToDesktopLocal(const QRect &rect);
    QImage grabDesktopImage(const QRect &desktopLocalRect = QRect());
	QVariantMap json2map(const QByteArray &val);
	QString map2json(const QVariantMap &val);
	QVariantList json2list(const QByteArray &val);
	QString list2json(const QVariantList &val);
    uint toKey(const QString& str);
    QString pixmapUniqueName(const QPixmap &pixmap);
    QString pixmapName();
    bool getRectFromCurrentPoint(WId hWndMySelf, QRect &out_rect);
    void cacheAllWindows();
    bool getSmallestWindowFromCursor(QRect &out_rect);
    QPoint fixPoint(const QPoint &point, const QSize &size);
    QRect fixWindowGeometry(const QRect &geometry, const QSize &minimumSize = QSize());
    QString strKeyEvent(QKeyEvent *key);
    QString strKeySequence(const QKeySequence &key);
	QByteArray pixmap2ByteArray(const QPixmap& pixmap, const char *format = "png");
    QByteArray image2ByteArray(const QImage &image, const char *format = "png");
	std::string getImageFormat(const char* data, int size);
    QString md5Pixmap(const QPixmap &pixmap);
    QString md5Image(const QImage &image);
    const QPixmap& multicolorCursorPixmap(); // 彩色光标原始图（96 DPI 基准）
    QCursor multicolorCursor(); // 按当前屏幕 DPI 缩放后的彩色光标
    double colorDistance(QColor e1, QColor e2); // 颜色相似度
    QColor colorOpposite(QColor clr); // 反色
    void setCurrentHwnd(WId hwnd);
    WId getCurrentHwnd();
    void intervalHandleOnce(const std::string &name, int msTime, const std::function<void()> &func);
    double getScreenScaleFactor(); // 获取屏幕DPI缩放因子
    double getScreenScaleFactor(const QPoint &globalPoint); // 获取指定屏幕位置的DPI缩放因子
    void enableDpiScalingForWindow(QWidget *widget); // 为特定窗口启用DPI缩放
    
    // DPI适配通用方法
    int scaleSize(int size); // 根据DPI缩放因子调整尺寸
    void scaleWidget(QWidget *widget, int baseWidth = 0, int baseHeight = 0); // 缩放窗口/控件大小
    void scaleFont(QWidget *widget); // 缩放控件字体
    void scaleFont(QFont &font); // 缩放字体对象
    void scaleFont(QFont &font, double scaleFactor); // 按指定屏幕缩放因子调整字体
    void scaleLayoutMargins(QLayout *layout, int left, int top, int right, int bottom); // 缩放布局边距
    void scaleWidgetDPI(QWidget *widget, int baseWidth = 0, int baseHeight = 0, bool scaleFont = true); // 综合DPI适配方法

    struct UploadResult {
        bool success;
        QString message; // Content (relative URL) or error message
        QString fullUrl; // If success
    };
    UploadResult uploadToGitHub(const QByteArray& imageData, const QString& fileName, const GitHubImageBedConfig& config);
}

template <typename F>
struct privDefer {
    F f;
    privDefer(F f) : f(f) {}
    ~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
    return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})

#define interval_handle_once(msTime, func)  Util::intervalHandleOnce(QString("%1%2").arg(__FILE__).arg(__LINE__).toStdString(), msTime, func);
