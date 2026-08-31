#pragma once

#include <QJsonArray>

class QString;

namespace LlmTools {

QJsonArray definitions();
QString execute(const QString &name, const QString &argumentsJson);

} // namespace LlmTools
