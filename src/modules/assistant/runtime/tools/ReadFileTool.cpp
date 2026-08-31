#include "ReadFileTool.h"

#include "LlmToolUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace {

constexpr int kDefaultReadMaxBytes = 32 * 1024;
constexpr int kMaxReadBytes = 256 * 1024;

} // namespace

namespace LlmTools {

    QString ReadFileTool::name() const
    {
        return QStringLiteral("read_file");
    }

    QString ReadFileTool::description() const
    {
        return QStringLiteral("读取本地文本文件内容，适合代码、配置文件、日志和文档。");
    }

    QJsonObject ReadFileTool::parameters() const
    {
        QJsonObject parametersObject;
        parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
        parametersObject.insert(QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("要读取的本地文件路径，支持相对路径和绝对路径")}
            }},
            {QStringLiteral("max_bytes"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"), QStringLiteral("最大读取字节数，默认 32768")}
            }}
            });
        parametersObject.insert(QStringLiteral("required"), QJsonArray{ QStringLiteral("path") });
        parametersObject.insert(QStringLiteral("additionalProperties"), false);
        return parametersObject;
    }

    QString ReadFileTool::execute(const QJsonObject& arguments) const
    {
        const QString rawPath = arguments.value(QStringLiteral("path")).toString().trimmed();
        const int maxBytes = clampInt(arguments.value(QStringLiteral("max_bytes")).toInt(kDefaultReadMaxBytes),
            1024,
            kMaxReadBytes);

        if (rawPath.isEmpty()) {
            return argumentError(QStringLiteral("缺少 path 参数"));
        }

        const QString absolutePath = QFileInfo(rawPath).isAbsolute()
            ? QFileInfo(rawPath).absoluteFilePath()
            : QDir::current().absoluteFilePath(rawPath);

        QFile file(absolutePath);
        if (!file.exists()) {
            return QStringLiteral(
                "# Read File Result\n\n"
                "- Path: `%1`\n"
                "- Error: 文件不存在")
                .arg(QDir::toNativeSeparators(absolutePath));
        }

        if (!file.open(QIODevice::ReadOnly)) {
            return QStringLiteral(
                "# Read File Result\n\n"
                "- Path: `%1`\n"
                "- Error: %2")
                .arg(QDir::toNativeSeparators(absolutePath), file.errorString());
        }

        const QByteArray data = file.read(maxBytes + 1);
        const bool truncated = data.size() > maxBytes || file.size() > maxBytes;
        const QByteArray visibleData = truncated ? data.left(maxBytes) : data;

        if (looksBinary(visibleData)) {
            return QStringLiteral(
                "# Read File Result\n\n"
                "- Path: `%1`\n"
                "- Size: %2 bytes\n"
                "- Error: 文件看起来是二进制内容，当前工具只适合读取文本文件")
                .arg(QDir::toNativeSeparators(absolutePath))
                .arg(file.size());
        }

        QString text = normalizeLineEndings(QString::fromUtf8(visibleData));
        if (truncated) {
            text += QStringLiteral("\n\n[文件内容已截断]");
        }

        return text;
    }

} 
// namespace LlmTools
