#pragma once

#include <QJsonObject>
#include <QList>
#include <QSharedPointer>
#include <QString>

class SettingModel;

namespace LlmTools {

struct ToolMeta {
    QString name;
    QString description;
};

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
    virtual bool requiresConfirmation() const;

    QJsonObject definition() const;
    bool disabled() const;
    void setDisabled(bool disabled);

protected:
    QString argumentError(const QString &message) const;

private:
    bool disabled_ = false;
};

QList<QSharedPointer<LlmTool>> createBuiltinTools(::SettingModel* settings,
                                                  const QStringList &disabledToolNames = {},
                                                  bool includeWebSearch = false);
QList<ToolMeta> builtinToolMetas(bool includeWebSearch = false);

} // namespace LlmTools
