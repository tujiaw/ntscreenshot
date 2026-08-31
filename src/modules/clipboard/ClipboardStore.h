#pragma once

#include "ClipItem.h"

#include <QString>
#include <vector>

// SQLite-backed persistence for clipboard history and settings.
// Schema and behaviour mirror wtl_clipboard's ClipboardStore (history.db under
// %LOCALAPPDATA%/ClipboardLite). Falls back to in-memory only when the DB is
// unavailable.
class ClipboardStore {
public:
    ClipboardStore();
    ~ClipboardStore();

    ClipboardStore(const ClipboardStore&) = delete;
    ClipboardStore& operator=(const ClipboardStore&) = delete;

    bool Available() const {
        return available_;
    }

    std::vector<ClipItem> Load(size_t maxItems);
    bool Save(const std::vector<ClipItem>& items, size_t maxItems);
    int LoadSettingInt(const char* name, int defaultValue);
    bool SaveSettingInt(const char* name, int value);
    QString LoadSettingText(const char* name, const QString& defaultValue);
    bool SaveSettingText(const char* name, const QString& value);

private:
    bool Open();
    bool EnsureSchema();
    void Close();

    static QString DefaultDatabasePath();

    bool available_ = false;
};
