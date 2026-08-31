#pragma once

#include "LlmTool.h"

class SettingModel;

namespace LlmTools {

class WebSearchTool : public LlmTool {
public:
    explicit WebSearchTool(::SettingModel* settings);

    QString name() const override;
    QString description() const override;
    QJsonObject parameters() const override;
    QString execute(const QJsonObject &arguments) const override;

private:
    ::SettingModel* settings_ = nullptr;
};

} // namespace LlmTools
