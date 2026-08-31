#include "app/crash/MiniDump.h"

#ifdef _WIN32
#include <QDebug>
#include <QDateTime>
#include <QDir>

void MiniDump::EnableAutoDump(bool bEnable)
{
    if (bEnable) {
        SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
    }
}

LONG MiniDump::ApplicationCrashHandler(EXCEPTION_POINTERS *pException)
{
    QString dumpDir = QDir::currentPath();
    QString dumpFile = QString("%1/ntscreenshot_%2.dmp").arg(dumpDir).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    std::wstring wDumpFile = dumpFile.toStdWString();
    CreateDumpFile(wDumpFile.c_str(), pException);

    QString msg = QString("I'm so sorry, but the program crashed.\r\ndump file : %1").arg(dumpFile);
    qDebug() << msg;
    FatalAppExitW(0, msg.toStdWString().c_str());

    return EXCEPTION_EXECUTE_HANDLER;
}

void MiniDump::CreateDumpFile(LPCWSTR strPath, EXCEPTION_POINTERS *pException)
{
    HANDLE hDumpFile = CreateFile(strPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
    dumpInfo.ExceptionPointers = pException;
    dumpInfo.ThreadId = GetCurrentThreadId();
    dumpInfo.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
    CloseHandle(hDumpFile);
}
#endif
