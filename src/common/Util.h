#pragma once

#include <QObject>
#include <QPixmap>
#include <functional>
#include "Constants.h"

class QWidget;
class QKeyEvent;
class QKeySequence;
namespace Util {
	QStringList getFiles(QString path, bool containsSubDir = true);
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
	QVariantMap json2map(const QByteArray &val);
	QString map2json(const QVariantMap &val);
	QVariantList json2list(const QByteArray &val);
	QString list2json(const QVariantList &val);
    uint toKey(const QString& str);
    QString pixmapUniqueName(const QPixmap &pixmap);
    QString pixmapName();
    bool getRectFromCurrentPoint(HWND hWndMySelf, QRect &out_rect);
    bool getSmallestWindowFromCursor(QRect &out_rect);
    QPoint fixPoint(const QPoint &point, const QSize &size);
    QString strKeyEvent(QKeyEvent *key);
    QString strKeySequence(const QKeySequence &key);
	QByteArray pixmap2ByteArray(const QPixmap& pixmap, const char *format = "png");
    QByteArray image2ByteArray(const QImage &image, const char *format = "png");
	std::string getImageFormat(const char* data, int size);
    QString md5Pixmap(const QPixmap &pixmap);
    QString md5Image(const QImage &image);
    const QPixmap& multicolorCursorPixmap(); // 彩色光标
    double colorDistance(QColor e1, QColor e2); // 颜色相似度
    QColor colorOpposite(QColor clr); // 反色
    void setCurrentHwnd(HWND hwnd);
    HWND getCurrentHwnd();
    void intervalHandleOnce(const std::string &name, int msTime, const std::function<void()> &func);
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