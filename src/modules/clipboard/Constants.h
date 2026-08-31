#pragma once

// Resource / command ids shared across the clipboard-lite submodule.

namespace cl::id {

// Tray menu commands
constexpr int TrayShow = 300;
constexpr int TrayClear = 301;
constexpr int TrayExit = 302;

constexpr int Max20 = 310;
constexpr int Max50 = 311;
constexpr int Max100 = 312;
constexpr int Max200 = 313;

constexpr int HotkeyCtrlBacktick = 325;
constexpr int HotkeyCtrlAltV = 320;
constexpr int HotkeyCtrlShiftV = 321;
constexpr int HotkeyAltShiftV = 322;
constexpr int HotkeyCtrlAltC = 323;
constexpr int HotkeyCtrlShiftC = 324;

constexpr int PasteHotkeyCtrlAltNum = 330;
constexpr int PasteHotkeyCtrlNum = 331;
constexpr int PasteHotkeyAltNum = 332;

constexpr int ImageScaleOriginal = 340;
constexpr int ImageScale640 = 341;
constexpr int ImageScale1024 = 342;
constexpr int ImageScale1600 = 343;

constexpr int AiFillSettings = 350;

// Native hotkey slot ids (passed to RegisterHotKey)
constexpr int HotkeyShowHistory = 200;
constexpr int HotkeyPasteFirst = 201;
constexpr int HotkeyPasteLast = 209;

// Timer ids
constexpr int TimerPasteAfterHotkey = 250;
constexpr int TimerRetryCapture = 251;
constexpr int TimerRefocusAfterPaste = 252;
constexpr int TimerPollSequence = 253;
constexpr int TimerTrackTarget = 254;
constexpr int TimerRetryTray = 255;
constexpr int TimerSyncStore = 256;
constexpr int TimerUpdateRelativeTime = 1;
constexpr int TimerHintHide = 2;

} // namespace cl::id
