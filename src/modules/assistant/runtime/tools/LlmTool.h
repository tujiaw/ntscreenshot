#pragma once

#include <QJsonObject>
#include <QString>

namespace LlmTools {

class ToolAbort;

class LlmTool {
public:
    explicit LlmTool(bool disabled = false);
    virtual ~LlmTool() = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QJsonObject parameters() const = 0;
    virtual QString execute(const QJsonObject &arguments) const = 0;
    virtual QString execute(const QJsonObject &arguments, ToolAbort *abort) const;

    QJsonObject definition() const;
    bool disabled() const;
    void setDisabled(bool disabled);

protected:
    QString argumentError(const QString &message) const;

private:
    bool disabled_ = false;
};

} // namespace LlmTools
