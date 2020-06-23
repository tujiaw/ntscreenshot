#pragma once

#include <QObject>
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
	QString getSystemDir(int csidl);
	QStringList getAllLnk();
	QVariantMap json2map(const QByteArray &val);
	QString map2json(const QVariantMap &val);
	QVariantList json2list(const QByteArray &val);
	QString list2json(const QVariantList &val);
    uint toKey(const QString& str);
	QString screenshotDefaultName();
    bool getSmallestWindowFromCursor(QRect &out_rect);
    QPoint fixPoint(const QPoint &point, const QSize &size);
    QString strKeyEvent(QKeyEvent *key);
    QString strKeySequence(const QKeySequence &key);
	QByteArray pixmap2ByteArray(const QPixmap& pixmap, const char *format = "jpg");
	std::wstring ansi2unicode(const std::string& ansi);
	std::string unicode2ansi(const std::wstring& unicode);
	std::string string_to_utf8(const std::string& srcStr);
	std::string utf8_to_string(const std::string& srcStr);
	std::wstring utf8_to_wstring(const std::string& str);
	std::string wstring_to_utf8(const std::wstring& str);
	std::string getImageFormat(const char* data, int size);
}
