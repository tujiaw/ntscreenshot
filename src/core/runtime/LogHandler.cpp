#include "LogHandler.h"
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDate>
#include <QDateTime>
#include <memory>
#include "core/platform/Util.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <debugapi.h>
#endif

namespace {

struct LogWrap {
    QMutex locker;
    QString date;
    std::unique_ptr<QFile> file;
    std::unique_ptr<QTextStream> ts;
};

LogWrap s_logWrap;

// 清理30天前的日志，每次只检查少量文件避免卡顿
void cleanOldLogs(const QString& logsDir) {
    static QDate lastCleanDate;
    QDate today = QDate::currentDate();
    
    // 每天只清理一次
    if (lastCleanDate == today)
        return;
    lastCleanDate = today;
    
    QDir dir(logsDir);
    if (!dir.exists())
        return;
        
    QString cutoffDate = QString("log%1.log").arg(today.addDays(-30).toString("yyyyMMdd"));
    const QStringList allLogs = dir.entryList(QStringList() << "log*.log", QDir::Files);
    
    for (const QString& fileName : allLogs) {
        if (fileName < cutoffDate) {
            QFile::remove(dir.absoluteFilePath(fileName));
        }
    }
}

// 获取当前时间字符串，格式固定避免重复构造
inline QString currentTimeStr() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

// 日志级别转字符串
inline const char* msgTypeToStr(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:    return "Debug:";
    case QtInfoMsg:     return "Info:";
    case QtWarningMsg:  return "Warning:";
    case QtCriticalMsg: return "Critical:";
    case QtFatalMsg:    return "Fatal:";
    }
    return "Unknown:";
}

// 检查并切换日志文件
bool ensureLogFileOpen(const QString& curDate) {
    if (s_logWrap.date == curDate && s_logWrap.ts) {
        return true;
    }
    
    // 关闭旧文件
    s_logWrap.ts.reset();
    s_logWrap.file.reset();
    s_logWrap.date.clear();
    
    QString logsDir = Util::getLogsDir();
    if (logsDir.isEmpty()) {
        return false;
    }
    
    cleanOldLogs(logsDir);
    
    QString logPath = QString("%1/log%2.log").arg(logsDir, curDate);
    auto file = std::make_unique<QFile>(logPath);
    
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        return false;
    }
    
    s_logWrap.file = std::move(file);
    s_logWrap.ts = std::make_unique<QTextStream>(s_logWrap.file.get());
    s_logWrap.date = curDate;
    
    return true;
}

} // namespace

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString curDate = QDate::currentDate().toString("yyyyMMdd");
    const QString text = QString("%1[%2:%3]%4%5\n")
                             .arg(currentTimeStr())
                             .arg(context.file ? context.file : "")
                             .arg(context.line)
                             .arg(msgTypeToStr(type))
                             .arg(msg);

#ifdef Q_OS_WIN
    OutputDebugStringW(reinterpret_cast<const wchar_t*>(text.utf16()));
#endif

    // 在同一个临界区内完成切日、写入和刷盘，避免跨天竞争导致日志写入错误文件。
    QMutexLocker locker(&s_logWrap.locker);
    if (!ensureLogFileOpen(curDate)) {
        return; // 无法打开日志文件，静默丢弃
    }

    *s_logWrap.ts << text;
    s_logWrap.ts->flush();
}
