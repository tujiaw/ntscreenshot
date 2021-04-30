#pragma once
#include <qdebug.h>
#include <vector>
#include <Windows.h>
#include "Singleton.h"

#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))

BOOL IsRectEmpty(RECT r)
{
    return r.bottom == 0 && r.left == 0 && r.right == 0 && r.top == 0;
}

//pop up win 1 (level 1).. first Z order
//        child11 (level 2)
//        child12 (level 2)
//                chilld121 (level 3)
//                chilld122 (level 3)
//                
//        child3 (level 2)
//pop up win2
//        child21 (level 2)
//        child21 (level 2)
// .
// .
//pop up winN . last Z order

struct HWndCacheInfo
{
    HWndCacheInfo() : hwnd(NULL) {}
    HWndCacheInfo(HWND _hwnd, RECT _rect, INT _level) : hwnd(_hwnd), rect(_rect), level(_level) {}
    BOOL isNull() const { return hwnd == NULL; }
    HWND hwnd;
    RECT rect;    //window rect
    INT level;    // 1 - pop up window  ;  2N - child window
};

class HWndRectCacheManager : public Singleton<HWndRectCacheManager>
{
public:
    BOOL cacheAll()
    {
        qDebug() << "cache all size:" << m_cacheList.size();
        m_cacheList.clear();
        // cache current window Z order when call this function
        EnumWindows(EnumWindowsSnapshotProc, 1);

        return TRUE;
    }

    void setFilterHWnd(HWND hwnd)
    {
        m_hWndFilter = hwnd;
    }

    HWND getWnd(POINT pt)
    {
        m_hWndTarget = NULL;
        EnumWindows(EnumWindowsRealTimeProc, MAKELPARAM(pt.x, pt.y));
        return m_hWndTarget;
    }

    RECT getWndRect(POINT ptHit, BOOL bGetInRealTime = FALSE)
    {
        RECT rtRect;
        rtRect.bottom = 0; rtRect.left = 0; rtRect.right = 0; rtRect.top = 0;
        //get from current Z order
        if (bGetInRealTime) {
            HWND hWndTarget = getWnd(ptHit);
            if (hWndTarget != NULL) {
                GetWindowRect(hWndTarget, &rtRect);
            }
        } else {
            //get from snapshot cache
            getRectFromCache(ptHit, rtRect);
        }

        return rtRect;
    }

protected:
    static BOOL CALLBACK EnumWindowsRealTimeProc(HWND hwnd, LPARAM lParam)
    {
        POINT p;
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        if (!HWndContainsPoint(hwnd, p)) return TRUE;
        if (HWndFilter(hwnd)) return TRUE;

        m_hWndTarget = hwnd;
        EnumChildWindows(hwnd, EnumChildRealTimeProc, lParam);
        return FALSE;
    }

    static BOOL CALLBACK EnumChildRealTimeProc(HWND hwnd, LPARAM lParam)
    {
        POINT p;
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        if (!HWndContainsPoint(hwnd, p)) return TRUE;
        if (HWndFilter(hwnd)) return TRUE;

        m_hWndTarget = hwnd;
        EnumChildWindows(hwnd, EnumChildRealTimeProc, lParam);
        return FALSE;
    }

protected:
    static BOOL CALLBACK EnumWindowsSnapshotProc(HWND hwnd, LPARAM lParam)
    {
        INT nLevel = lParam;
        if (HWndFilter(hwnd)) return TRUE;
        saveHWndRect(hwnd, nLevel);
        EnumChildWindows(hwnd, EnumChildSnapshotProc, ++nLevel);
        return TRUE;
    }

    static BOOL CALLBACK EnumChildSnapshotProc(HWND hwnd, LPARAM lParam)
    {
        INT nLevel = lParam;
        if (HWndFilter(hwnd)) return TRUE;

        saveHWndRect(hwnd, nLevel);
        ++nLevel;
        EnumChildWindows(hwnd, EnumChildSnapshotProc, nLevel);
        return TRUE;
    }

protected:
    static BOOL HWndContainsPoint(HWND hWnd, POINT pt)
    {
        RECT rtWin;
        rtWin.bottom = 0; rtWin.left = 0; rtWin.right = 0; rtWin.top = 0;
        GetWindowRect(hWnd, &rtWin);
        return PtInRect(&rtWin, pt);
    }

    static BOOL HWndFilter(HWND hWnd)
    {
        if (m_hWndFilter && m_hWndFilter == hWnd) {
            return TRUE;
        }

        DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
        DWORD dwStyleMust = WS_VISIBLE;
        if ((dwStyle & dwStyleMust) != dwStyleMust) {
            return TRUE;
        }

        DWORD dwStyleEx = GetWindowLong(hWnd, GWL_EXSTYLE);
        DWORD dwStyleMustNot = WS_EX_TRANSPARENT;
        if ((dwStyleMustNot & dwStyleEx) != 0) {
            return TRUE;
        }

        return FALSE;
    }

    //find the first window that level is biggest
    static BOOL  getRectFromCache(POINT point, RECT& rect)
    {
        HWndCacheInfo targetInfo;
        for (const auto &info : m_cacheList) {
            if (!targetInfo.isNull() && info.level <= targetInfo.level) {
                break;
            }
            if (PtInRect(&info.rect, point)) {
                if (targetInfo.isNull()) {
                    targetInfo = info;
                } else {
                    if (info.level > targetInfo.level) {
                        targetInfo = info;
                    }
                }
            }
        }
        if (!targetInfo.isNull()) {
            rect = targetInfo.rect;
            return TRUE;
        }
        return FALSE;
    }

    static VOID saveHWndRect(HWND hWnd, INT nLevel)
    {
        _ASSERTE(hWnd != NULL && IsWindow(hWnd));
        RECT rect;
        if (GetWindowRect(hWnd, &rect)) {
            HWndCacheInfo info(hWnd, rect, nLevel);
            if (!info.isNull()) {
                m_cacheList.push_back(info);
            }
        }
    }

protected:
    friend class Singleton<HWndRectCacheManager>;

    static HWND m_hWndTarget;
    static HWND m_hWndFilter;
    static std::vector<HWndCacheInfo> m_cacheList;
};

HWND HWndRectCacheManager::m_hWndTarget = NULL;
HWND HWndRectCacheManager::m_hWndFilter = NULL;
std::vector<HWndCacheInfo> HWndRectCacheManager::m_cacheList;
