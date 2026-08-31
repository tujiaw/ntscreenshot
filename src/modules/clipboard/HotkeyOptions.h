#pragma once

#include "Constants.h"

#include <windows.h>

#include <array>
#include <QString>

// Hotkey / option tables. Virtual-key codes and MOD_* flags are native Win32
// values used with RegisterHotKey, mirroring wtl_clipboard's AppOptions.

struct HotkeyOption {
    int commandId;
    UINT modifiers;
    UINT key;
    QString label;
};

struct PasteHotkeyOption {
    int commandId;
    UINT modifiers;
    QString label;
};

struct ImageScaleOption {
    int commandId;
    int maxEdge;
    QString label;
};

inline const std::array<HotkeyOption, 6>& ShowHotkeyOptions() {
    static const std::array<HotkeyOption, 6> options{ {
        { cl::id::HotkeyCtrlBacktick, MOD_CONTROL, VK_OEM_3, QStringLiteral("Ctrl+`") },
        { cl::id::HotkeyCtrlAltV, MOD_CONTROL | MOD_ALT, 'V', QStringLiteral("Ctrl+Alt+V") },
        { cl::id::HotkeyCtrlShiftV, MOD_CONTROL | MOD_SHIFT, 'V', QStringLiteral("Ctrl+Shift+V") },
        { cl::id::HotkeyAltShiftV, MOD_ALT | MOD_SHIFT, 'V', QStringLiteral("Alt+Shift+V") },
        { cl::id::HotkeyCtrlAltC, MOD_CONTROL | MOD_ALT, 'C', QStringLiteral("Ctrl+Alt+C") },
        { cl::id::HotkeyCtrlShiftC, MOD_CONTROL | MOD_SHIFT, 'C', QStringLiteral("Ctrl+Shift+C") },
    } };
    return options;
}

inline const std::array<PasteHotkeyOption, 3>& PasteHotkeyOptions() {
    static const std::array<PasteHotkeyOption, 3> options{ {
        { cl::id::PasteHotkeyCtrlAltNum, MOD_CONTROL | MOD_ALT, QStringLiteral("Ctrl+Alt+1..9") },
        { cl::id::PasteHotkeyCtrlNum, MOD_CONTROL, QStringLiteral("Ctrl+1..9") },
        { cl::id::PasteHotkeyAltNum, MOD_ALT, QStringLiteral("Alt+1..9") },
    } };
    return options;
}

inline const std::array<ImageScaleOption, 4>& ImageScaleOptions() {
    static const std::array<ImageScaleOption, 4> options{ {
        { cl::id::ImageScaleOriginal, 0, QStringLiteral("Original size") },
        { cl::id::ImageScale640, 640, QStringLiteral("Fit within 640 px") },
        { cl::id::ImageScale1024, 1024, QStringLiteral("Fit within 1024 px") },
        { cl::id::ImageScale1600, 1600, QStringLiteral("Fit within 1600 px") },
    } };
    return options;
}

inline const HotkeyOption* FindShowHotkey(int commandId) {
    for (const HotkeyOption& option : ShowHotkeyOptions()) {
        if (option.commandId == commandId) {
            return &option;
        }
    }
    return nullptr;
}

inline const PasteHotkeyOption* FindPasteHotkey(int commandId) {
    for (const PasteHotkeyOption& option : PasteHotkeyOptions()) {
        if (option.commandId == commandId) {
            return &option;
        }
    }
    return nullptr;
}

inline const ImageScaleOption* FindImageScale(int commandId) {
    for (const ImageScaleOption& option : ImageScaleOptions()) {
        if (option.commandId == commandId) {
            return &option;
        }
    }
    return nullptr;
}

inline const ImageScaleOption* FindImageScaleByMaxEdge(int maxEdge) {
    for (const ImageScaleOption& option : ImageScaleOptions()) {
        if (option.maxEdge == maxEdge) {
            return &option;
        }
    }
    return nullptr;
}

inline QString ShowHotkeyLabel(int registeredCommandId, int preferredCommandId) {
    const HotkeyOption* option = FindShowHotkey(registeredCommandId);
    if (option) {
        return option->label;
    }
    option = FindShowHotkey(preferredCommandId);
    return option ? option->label : QStringLiteral("hotkey unavailable");
}

inline QString PasteHotkeyLabel(int registeredCommandId, int preferredCommandId) {
    const PasteHotkeyOption* option = FindPasteHotkey(registeredCommandId);
    if (option) {
        return option->label;
    }
    option = FindPasteHotkey(preferredCommandId);
    return option ? option->label : QStringLiteral("paste hotkey unavailable");
}

inline QString ImageScaleLabel(int maxEdge) {
    const ImageScaleOption* option = FindImageScaleByMaxEdge(maxEdge);
    return option ? option->label : QStringLiteral("original size");
}
