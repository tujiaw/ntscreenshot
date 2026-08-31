#include "LlmToolUtils.h"
#include "ToolAbort.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>

namespace LlmTools {

int clampInt(int value, int minValue, int maxValue)
{
    return qMax(minValue, qMin(value, maxValue));
}

QString normalizeLineEndings(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString collapseBlankLines(QString text)
{
    text = normalizeLineEndings(text);
    text.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
    return text.trimmed();
}

QString truncateText(const QString &text, int maxChars, bool *truncatedOut)
{
    const bool truncated = text.size() > maxChars;
    if (truncatedOut) {
        *truncatedOut = truncated;
    }

    if (!truncated) {
        return text;
    }

    QString result = text.left(maxChars);
    result += QStringLiteral("\n\n[内容已截断]");
    return result;
}

bool waitForProcess(QProcess *process, int timeoutMs, ToolAbort *abort)
{
    if (!process) {
        return false;
    }
    if (abort) {
        abort->registerProcess(process);
        if (abort->isAborted()) {
            process->kill();
            process->waitForFinished();
            abort->unregisterProcess(process);
            return false;
        }
    }

    const bool finished = process->waitForFinished(timeoutMs);
    if (abort) {
        abort->unregisterProcess(process);
        if (abort->isAborted()) {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
                process->waitForFinished();
            }
            return false;
        }
    }
    if (!finished) {
        process->kill();
        process->waitForFinished();
        return false;
    }
    return true;
}

QString stripAnsi(QString text)
{
    return text.remove(QRegularExpression(QStringLiteral("\\x1b\\[[0-9;?]*[ -/]*[@-~]")));
}

QString markdownFenceLanguageForFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("cpp") || suffix == QStringLiteral("cc") || suffix == QStringLiteral("cxx")) return QStringLiteral("cpp");
    if (suffix == QStringLiteral("c")) return QStringLiteral("c");
    if (suffix == QStringLiteral("h") || suffix == QStringLiteral("hpp") || suffix == QStringLiteral("hh") || suffix == QStringLiteral("hxx")) return QStringLiteral("cpp");
    if (suffix == QStringLiteral("py")) return QStringLiteral("python");
    if (suffix == QStringLiteral("js")) return QStringLiteral("javascript");
    if (suffix == QStringLiteral("ts")) return QStringLiteral("typescript");
    if (suffix == QStringLiteral("json")) return QStringLiteral("json");
    if (suffix == QStringLiteral("md")) return QStringLiteral("markdown");
    if (suffix == QStringLiteral("xml") || suffix == QStringLiteral("ui") || suffix == QStringLiteral("vcxproj") || suffix == QStringLiteral("filters")) return QStringLiteral("xml");
    if (suffix == QStringLiteral("yml") || suffix == QStringLiteral("yaml")) return QStringLiteral("yaml");
    if (suffix == QStringLiteral("ps1")) return QStringLiteral("powershell");
    if (suffix == QStringLiteral("bat")) return QStringLiteral("bat");
    if (suffix == QStringLiteral("html") || suffix == QStringLiteral("htm")) return QStringLiteral("html");
    if (suffix == QStringLiteral("css")) return QStringLiteral("css");
    if (suffix == QStringLiteral("sh")) return QStringLiteral("bash");
    if (suffix == QStringLiteral("txt")) return QStringLiteral("text");
    return QStringLiteral("text");
}

bool looksBinary(const QByteArray &data)
{
    if (data.isEmpty()) {
        return false;
    }

    int controlCount = 0;
    const int sampleSize = qMin(data.size(), 1024);
    for (int i = 0; i < sampleSize; ++i) {
        const unsigned char ch = static_cast<unsigned char>(data.at(i));
        if (ch == 0) {
            return true;
        }
        if (ch < 0x09 || (ch > 0x0D && ch < 0x20)) {
            ++controlCount;
        }
    }

    return controlCount > sampleSize / 10;
}

QJsonObject parseArgumentsJson(const QString &argumentsJson, QString *errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(argumentsJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = QStringLiteral("工具参数不是合法 JSON：%1").arg(parseError.errorString());
        }
        return {};
    }

    return doc.object();
}

} // namespace LlmTools
