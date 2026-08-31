#pragma once

#include "LlmTool.h"

namespace LlmTools {

class EditFileTool : public LlmTool {
public:
    QString name() const override;
    QString description() const override;
    QJsonObject parameters() const override;
    QString execute(const QJsonObject &arguments) const override;
    bool requiresConfirmation() const override { return true; }
};

} // namespace LlmTools
