#pragma once

#include "ClipboardStore.h"
#include "LlmClient.h"

#include <QString>

struct AiFillSettings {
    QString apiEndpoint = QStringLiteral("https://api.deepseek.com");
    QString apiKey;
    QString modelName = QStringLiteral("deepseek-v4-flash");
    bool enabled = false;

    void Load(ClipboardStore& store);
    void Save(ClipboardStore& store) const;
    LlmConfig ToLlmConfig() const;
    bool IsConfigured() const;
};
