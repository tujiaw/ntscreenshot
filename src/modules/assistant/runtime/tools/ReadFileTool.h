#pragma once

#include "LlmTool.h"

namespace LlmTools {

class ReadFileTool : public LlmTool {
public:
    QString name() const override;
    QString description() const override;
    QJsonObject parameters() const override;
    QString execute(const QJsonObject &arguments) const override;
};

} // namespace LlmTools
