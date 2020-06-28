#include "Util.h"
#include <QStringList>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QWidget>
#include <QApplication>
#include <QDesktopWidget>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QKeyEvent>
#include <QKeySequence>
#include <QBuffer>
#include <QImageReader>
#include <QStandardPaths>
#include <QTimer>
#include <QCryptographicHash>
#pragma warning(disable:4091)
#include <ShlObj.h>
#pragma comment(lib, "Shell32.lib")

/*
* DWORD EnumerateFileInDirectory(LPSTR szPath)
* 功能：遍历目录下的文件和子目录，将显示文件和文件夹隐藏、加密的属性
*
* 参数：LPSTR szPath，为需遍历的路径
*
* 返回值：0代表执行完成，1代表发送错误
*/

void EnumerateFileInDirectory(const QString &dir, bool containsSubDir, QStringList &result)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hListFile;
	WCHAR szFilePath[MAX_PATH];

	// 构造代表子目录和文件夹路径的字符串，使用通配符"*"
	lstrcpy(szFilePath, dir.toStdWString().c_str());
	// 注释的代码可以用于查找所有以“.txt”结尾的文件
	// lstrcat(szFilePath, "\\*.txt");
	lstrcat(szFilePath, L"\\*");

	// 查找第一个文件/目录，获得查找句柄
	hListFile = FindFirstFile(szFilePath, &FindFileData);
	// 判断句柄
	if (hListFile == INVALID_HANDLE_VALUE) {
		qDebug() << "error:" << GetLastError();
	} else {
		do {
			if(lstrcmp(FindFileData.cFileName, TEXT(".")) == 0 || lstrcmp(FindFileData.cFileName, TEXT("..")) == 0) {
				continue;
			}
			
			// 判断文件属性，是否为加密文件或者文件夹
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) {
				continue;
			}
			// 判断文件属性，是否为隐藏文件或文件夹
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
				continue;
			}

			QString path = dir + "\\" + QString::fromStdWString(FindFileData.cFileName);
			// 判断文件属性，是否为目录
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (containsSubDir) {
					EnumerateFileInDirectory(path, containsSubDir, result);
				}
				continue;
			}
			// 读者可根据文件属性表中的内容自行添加、判断文件属性
			result.push_back(path);
		} while (FindNextFile(hListFile, &FindFileData));
	}
}

namespace Util
{

	QStringList getFiles(QString path, bool containsSubDir)
	{
		QStringList result;
		QDirIterator curit(path, QStringList(), QDir::Files);
		while (curit.hasNext()) {
			result.push_back(curit.next());
		}

		if (containsSubDir) {
			QDirIterator subit(path, QStringList(), QDir::Files, QDirIterator::Subdirectories);
			while (subit.hasNext()) {
				result.push_back(subit.next());
			}
		}

		return result;
	}


	bool shellExecute(const QString &path)
	{
        return shellExecute(path, "open");
	}

    bool shellExecute(const QString &path, const QString &operation)
    {
        if (path.isEmpty()) {
            return false;
        }

        HINSTANCE hinst = ShellExecute(NULL, operation.toStdWString().c_str(), path.toStdWString().c_str(), NULL, NULL, SW_SHOWNORMAL);
        LONG64 result = (LONG64)hinst;
        if (result <= 32) {
            qDebug() << "shellExecute failed, code:" << result;
            return false;
        }
        return true;
    }

	bool locateFile(const QString &dir)
	{
        if (dir.isEmpty()) {
            return false;
        }

		QString newDir = dir;
		QString cmd = QString("/select, \"%1\"").arg(newDir.replace("/", "\\"));
		qDebug() << cmd;

		HINSTANCE hinst = ::ShellExecute(NULL, L"open", L"explorer.exe", cmd.toStdWString().c_str(), NULL, SW_SHOW);
		LONG64 result = (LONG64)hinst;
		if (result <= 32) {
			qDebug() << "locateFile failed, code:" << result;
			return false;
		}
		return true;
	}

	void setForegroundWindow(QWidget *widget)
	{
		if (widget) {
			HWND hwnd = (HWND)widget->winId();
			::SetForegroundWindow(hwnd);
		}
	}

	void setWndTopMost(QWidget *widget)
	{
		if (widget) {
			HWND hwnd = (HWND)widget->winId();
			RECT rect;
			GetWindowRect(hwnd, &rect);
			SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, abs(rect.right - rect.left), abs(rect.bottom - rect.top), SWP_SHOWWINDOW);
		}
	}

	void cancelTopMost(QWidget *widget)
	{
		if (widget) {
			HWND hwnd = (HWND)widget->winId();
			RECT rect;
			GetWindowRect(hwnd, &rect);
			SetWindowPos(hwnd, HWND_NOTOPMOST, rect.left, rect.top, abs(rect.right - rect.left), abs(rect.bottom - rect.top), SWP_SHOWWINDOW);
		}
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
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
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

	QString getSystemDir(int csidl)
	{
		//CSIDL_COMMON_PROGRAMS CSIDL_PROGRAMS CSIDL_DESKTOP
		int csidls[] = { CSIDL_ADMINTOOLS, CSIDL_ALTSTARTUP, CSIDL_APPDATA, CSIDL_BITBUCKET, CSIDL_CDBURN_AREA,
			CSIDL_COMMON_ADMINTOOLS, CSIDL_COMMON_ALTSTARTUP, CSIDL_COMMON_APPDATA, CSIDL_COMMON_DESKTOPDIRECTORY,
			CSIDL_COMMON_DOCUMENTS, CSIDL_COMMON_FAVORITES, CSIDL_COMMON_MUSIC, CSIDL_COMMON_OEM_LINKS,
			CSIDL_COMMON_PICTURES, CSIDL_COMMON_PROGRAMS, CSIDL_COMMON_STARTMENU, CSIDL_COMMON_STARTUP,
			CSIDL_COMMON_TEMPLATES, CSIDL_COMMON_VIDEO, CSIDL_COMPUTERSNEARME, CSIDL_CONNECTIONS, CSIDL_CONTROLS,
			CSIDL_COOKIES, CSIDL_DESKTOP, CSIDL_DESKTOPDIRECTORY, CSIDL_DRIVES, CSIDL_FAVORITES, CSIDL_FONTS,
			CSIDL_HISTORY, CSIDL_INTERNET, CSIDL_INTERNET_CACHE, CSIDL_LOCAL_APPDATA, CSIDL_MYDOCUMENTS,
			CSIDL_MYMUSIC, CSIDL_MYPICTURES, CSIDL_MYVIDEO, CSIDL_NETHOOD, CSIDL_NETWORK, CSIDL_PERSONAL,
			CSIDL_PRINTERS, CSIDL_PRINTHOOD, CSIDL_PROFILE, CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86,
			CSIDL_PROGRAM_FILES_COMMON, CSIDL_PROGRAM_FILES_COMMONX86, CSIDL_PROGRAMS, CSIDL_RECENT, CSIDL_RESOURCES,
			CSIDL_RESOURCES_LOCALIZED, CSIDL_SENDTO, CSIDL_STARTMENU, CSIDL_STARTUP, CSIDL_SYSTEM, CSIDL_SYSTEMX86,
			CSIDL_TEMPLATES, CSIDL_WINDOWS, CSIDL_FLAG_DONT_UNEXPAND, CSIDL_FLAG_DONT_VERIFY, CSIDL_FLAG_NO_ALIAS,
			CSIDL_FLAG_PER_USER_INIT, CSIDL_FLAG_MASK
		};

		LPITEMIDLIST pidl;
		LPMALLOC pShellMalloc;
		wchar_t szDir[MAX_PATH] = { 0 };
		if (SUCCEEDED(SHGetMalloc(&pShellMalloc))) {
			if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, csidl, &pidl))) {
				SHGetPathFromIDList(pidl, szDir);
				pShellMalloc->Free(pidl);
			}
			pShellMalloc->Release();
		}
		return QString::fromStdWString(szDir);
	}

	QStringList getAllLnk()
	{
		QStringList result;
		QString desktop = getSystemDir(CSIDL_DESKTOP);
		QString commonPrograms = getSystemDir(CSIDL_COMMON_PROGRAMS);
		QString programs = getSystemDir(CSIDL_PROGRAMS);
        QString commonDesktop = getSystemDir(CSIDL_COMMON_DESKTOPDIRECTORY);

		result.append(getFiles(desktop, false));
		result.append(getFiles(commonPrograms));
		result.append(getFiles(programs));
        result.append(getFiles(commonDesktop));

		//EnumerateFileInDirectory(desktop, false, result);
		//EnumerateFileInDirectory(commonPrograms, true, result);
		//EnumerateFileInDirectory(programs, true, result);

		return result;
	}
	QVariantMap json2map(const QByteArray &val)
	{
		QJsonParseError jError;
		QJsonDocument jDoc = QJsonDocument::fromJson(val, &jError);
		if (jError.error == QJsonParseError::NoError) {
			if (jDoc.isObject()) {
				QJsonObject jObj = jDoc.object();
				return jObj.toVariantMap();
			}
		}
		QVariantMap ret;
		return ret;
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
		if (jError.error == QJsonParseError::NoError) {
			if (jDoc.isArray()) {
				QJsonArray jArr = jDoc.array();
				return jArr.toVariantList();
			}
		}
		QVariantList ret;
		return ret;
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

    bool getSmallestWindowFromCursor(QRect &out_rect)
    {
        HWND hwnd;
        POINT pt;
        // 获得当前鼠标位置
        ::GetCursorPos(&pt);
        // 获得当前位置桌面上的子窗口
        hwnd = ::ChildWindowFromPointEx(::GetDesktopWindow(),
            pt, CWP_SKIPDISABLED | CWP_SKIPINVISIBLE);
        if (hwnd != NULL) {
            HWND temp_hwnd;
            temp_hwnd = hwnd;
            while (true) {
                ::GetCursorPos(&pt);
                ::ScreenToClient(temp_hwnd, &pt);
                temp_hwnd = ::ChildWindowFromPointEx(temp_hwnd,
                    pt, CWP_SKIPINVISIBLE);
                if (temp_hwnd == NULL || temp_hwnd == hwnd)
                    break;
                hwnd = temp_hwnd;
            }
            RECT temp_window;
            ::GetWindowRect(hwnd, &temp_window);
            out_rect.setRect(temp_window.left, temp_window.top,
                temp_window.right - temp_window.left,
                temp_window.bottom - temp_window.top);
            return true;
        }
        return false;
    }

    QPoint fixPoint(const QPoint &point, const QSize &size)
    {
        QRect screenRect = qApp->desktop()->rect();
        int x = point.x();
        int y = point.y();
        if (x < screenRect.x()) {
            x = screenRect.x();
        } else if (x + size.width() > screenRect.width()) {
            x = screenRect.width() - size.width();
        }
        if (y + size.height() > screenRect.y()) {
            y = screenRect.y();
        } else if (y + size.height() > screenRect.height()) {
            y = screenRect.height() - size.height();
        }
        return QPoint(x, y);
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

	std::wstring ansi2unicode(const std::string& ansi)
	{
		if (ansi.empty()) {
			return std::wstring(L"");
		}
		int len = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, NULL, 0);
		std::wstring unicode(len + 1, L'\0');
		len = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), ansi.size(), &unicode[0], len);
		return unicode;
	}

	std::string unicode2ansi(const std::wstring& unicode)
	{
		if (unicode.empty()) {
			return std::string("");
		}
		int len = WideCharToMultiByte(CP_ACP, 0, unicode.c_str(), -1, NULL, 0, NULL, NULL);
		std::string ansi(len + 1, '\0');
		WideCharToMultiByte(CP_ACP, 0, unicode.c_str(), unicode.size(), &ansi[0], len, NULL, NULL);
		return ansi;
	}

	std::string string_to_utf8(const std::string& srcStr)
	{
		if (srcStr.empty())
		{
			return "";
		}

		int nwLen = MultiByteToWideChar(CP_ACP, 0, srcStr.c_str(), -1, NULL, 0);
		wchar_t* pwBuf = new wchar_t[nwLen + 1];
		ZeroMemory(pwBuf, nwLen * 2 + 2);
		MultiByteToWideChar(CP_ACP, 0, srcStr.c_str(), srcStr.length(), pwBuf, nwLen);
		int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);
		char* pBuf = new char[nLen + 1];
		ZeroMemory(pBuf, nLen + 1);
		WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);
		std::string retStr(pBuf);
		delete[]pwBuf;
		delete[]pBuf;
		pwBuf = NULL;
		pBuf = NULL;
		return retStr;
	}

	std::string utf8_to_string(const std::string& srcStr)
	{
		if (srcStr.empty())
		{
			return "";
		}

		int nwLen = MultiByteToWideChar(CP_UTF8, 0, srcStr.c_str(), -1, NULL, 0);
		wchar_t* pwBuf = new wchar_t[nwLen + 1];
		memset(pwBuf, 0, nwLen * 2 + 2);
		MultiByteToWideChar(CP_UTF8, 0, srcStr.c_str(), srcStr.length(), pwBuf, nwLen);
		int nLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, -1, NULL, NULL, NULL, NULL);
		char* pBuf = new char[nLen + 1];
		memset(pBuf, 0, nLen + 1);
		WideCharToMultiByte(CP_ACP, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);
		std::string retStr = pBuf;
		delete[]pBuf;
		delete[]pwBuf;
		pBuf = NULL;
		pwBuf = NULL;
		return retStr;
	}

	std::wstring utf8_to_wstring(const std::string& str)
	{
		int len = str.size();
		int unicodeLen = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
		wchar_t* pUnicode;
		pUnicode = new wchar_t[unicodeLen + 1];
		memset((void*)pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
		::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)pUnicode, unicodeLen);
		std::wstring wstrReturn(pUnicode);
		delete[] pUnicode;
		return wstrReturn;
	}

	std::string wstring_to_utf8(const std::wstring& str)
	{
		char* pElementText = NULL;
		int iTextLen = ::WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)str.c_str(), -1, NULL, 0, NULL, NULL);
		pElementText = new char[iTextLen + 1];
		memset((void*)pElementText, 0, (iTextLen + 1) * sizeof(char));
		::WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)str.c_str(), -1, pElementText, iTextLen, NULL, NULL);
		std::string strReturn(pElementText);
		delete[] pElementText;
		return strReturn;
	}

	std::string getImageFormat(const char* data, int size)
	{
		if (size < 8) {
			return "";
		}

		uchar png_type[9] = { 0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,'/0' };
		uchar file_head[9];
		for (int i = 0; i < 8; ++i) {
			file_head[i] = data[i];
		}

		file_head[8] = '/0';
		switch (file_head[0])
		{
		case 0xff:
			if (file_head[1] == 0xd8)
				return "jpg";
		case 0x42:
			if (file_head[1] == 0x4D)
				return "bmp";
		case 0x89:
			if (file_head[1] == png_type[1] && file_head[2] == png_type[2] && file_head[3] == png_type[3] && file_head[4] == png_type[4] &&
				file_head[5] == png_type[5] && file_head[6] == png_type[6] && file_head[7] == png_type[7])
				return "png";
		default:
			return "";
		}
	}

	UINT32 GetFileSize(const std::string& filePath)
	{
		UINT32 size = 0;
		FILE* pFile = NULL;
		_wfopen_s(&pFile, utf8_to_wstring(filePath).c_str(), L"rb");
		if (NULL != pFile)
		{
			fseek(pFile, 0, SEEK_END);
			size = ftell(pFile);
			fclose(pFile);
		}
		return size;
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
            QObject::connect(timer, &QTimer::timeout, [name, func]() {
                auto iter = s_intervalHandleTimers.find(name);
                if (iter != s_intervalHandleTimers.end()) {
                    if (iter.value()) {
                        iter.value()->stop();
                        iter.value()->deleteLater();
                    }
                    s_intervalHandleTimers.erase(iter);
                }
                func();
            });
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
}
