#include "AiFillSettings.h"

void AiFillSettings::Load(ClipboardStore& store) {
    enabled = store.LoadSettingInt("AiFillEnabled", 0) != 0;
    apiEndpoint = store.LoadSettingText("AiFillEndpoint", QStringLiteral("https://api.deepseek.com"));
    apiKey = store.LoadSettingText("AiFillApiKey", QString());
    modelName = store.LoadSettingText("AiFillModel", QStringLiteral("deepseek-v4-flash"));
}

void AiFillSettings::Save(ClipboardStore& store) const {
    store.SaveSettingInt("AiFillEnabled", enabled ? 1 : 0);
    store.SaveSettingText("AiFillEndpoint", apiEndpoint);
    store.SaveSettingText("AiFillApiKey", apiKey);
    store.SaveSettingText("AiFillModel", modelName);
}

LlmConfig AiFillSettings::ToLlmConfig() const {
    LlmConfig config;
    config.endpoint = apiEndpoint;
    config.apiKey = apiKey;
    config.model = modelName;
    return config;
}

bool AiFillSettings::IsConfigured() const {
    return enabled && !apiEndpoint.isEmpty() && !apiKey.isEmpty();
}
