#pragma once

#include <QJsonObject>
#include <QString>

class QByteArray;
class QProcess;

namespace LlmTools {

class ToolAbort;

int clampInt(int value, int minValue, int maxValue);
QString normalizeLineEndings(QString text);
QString collapseBlankLines(QString text);
QString truncateText(const QString &text, int maxChars, bool *truncatedOut = nullptr);
constexpr int kToolOutputMaxChars = 20 * 1024;
bool waitForProcess(QProcess *process, int timeoutMs, ToolAbort *abort);
QString stripAnsi(QString text);
QString markdownFenceLanguageForFile(const QString &path);
bool looksBinary(const QByteArray &data);
QJsonObject parseArgumentsJson(const QString &argumentsJson, QString *errorOut);

} // namespace LlmTools
