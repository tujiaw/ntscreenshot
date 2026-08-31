#pragma once

#include <windows.h>

#include <QImage>
#include <QString>

namespace WinClipboard {

// Read the current clipboard contents.
QString ReadText();
QImage ReadImage();

// Write an item back to the clipboard (scaling images to maxEdge when > 0).
bool SetText(const QString& text);
bool SetImage(const QImage& image, int maxEdge);

// Paste simulation (mirrors wtl_clipboard's SendInput logic).
void SendShiftInsert();
void SendPasteForHotkey(UINT releaseModifiers);

// Restore focus to the previously captured target window before pasting.
void RestoreFocus(HWND targetWindow, HWND targetFocusWindow);

// Returns the foreground window if it belongs to another process, else nullptr.
HWND ForegroundExternalWindow();

// Returns the focus window for a given target window (may be nullptr).
HWND FocusWindowOf(HWND targetWindow);

// True if `window` matches the primary window or belongs to the secondary window.
bool IsOwnWindow(HWND window, HWND primaryWindow, HWND secondaryWindow);

// Current clipboard sequence number (0 on non-Windows / unavailable).
DWORD ClipboardSequence();

} // namespace WinClipboard
