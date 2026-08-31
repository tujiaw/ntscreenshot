#pragma once

#ifdef _WIN32
#include <Windows.h>
#include <tchar.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

#ifdef UNICODE
#define TSprintf	wsprintf
#else
#define TSprintf	sprintf
#endif

class MiniDump
{
public:
    static void EnableAutoDump(bool bEnable = true);

private:
    static LONG ApplicationCrashHandler(EXCEPTION_POINTERS *pException);
    static void CreateDumpFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS *pException);
};
#else
class MiniDump
{
public:
    static void EnableAutoDump(bool = true) {}
};
#endif
