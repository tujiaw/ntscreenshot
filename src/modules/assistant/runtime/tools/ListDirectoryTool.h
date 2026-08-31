#pragma once

#include "LlmTool.h"

#include <QDir>
#include <QStringList>

namespace LlmTools {

class ListDirectoryTool : public LlmTool {
public:
    QString name() const override;
    QString description() const override;
    QJsonObject parameters() const override;
    QString execute(const QJsonObject &arguments) const override;

private:
    QStringList recursiveList(const QDir &dir, const QString &basePath,
                              QDir::Filters filters, QDir::SortFlags sort,
                              int maxEntries) const;
};

} // namespace LlmTools
