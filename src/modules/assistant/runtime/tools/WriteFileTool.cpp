#include "WriteFileTool.h"

#include "LlmToolUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace LlmTools {

QString WriteFileTool::name() const
{
    return QStringLiteral("write_file");
}

QString WriteFileTool::description() const
{
    return QStringLiteral("创建或覆盖写入文本文件。适合创建新文件、生成代码、写入配置。");
}

QJsonObject WriteFileTool::parameters() const
{
    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), QJsonObject{
        {QStringLiteral("path"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要写入的文件路径，支持相对路径和绝对路径")}
        }},
        {QStringLiteral("content"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要写入的文本内容")}
        }},
        {QStringLiteral("create_dirs"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("boolean")},
            {QStringLiteral("description"), QStringLiteral("如果父目录不存在是否自动创建，默认 true")}
        }}
    });
    parametersObject.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("path"), QStringLiteral("content")});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString WriteFileTool::execute(const QJsonObject &arguments) const
{
    const QString rawPath = arguments.value(QStringLiteral("path")).toString().trimmed();
    const QString content = arguments.value(QStringLiteral("content")).toString();
    const bool createDirs = arguments.value(QStringLiteral("create_dirs")).toBool(true);

    if (rawPath.isEmpty()) {
        return argumentError(QStringLiteral("缺少 path 参数"));
    }
    if (!arguments.contains(QStringLiteral("content"))) {
        return argumentError(QStringLiteral("缺少 content 参数"));
    }

    const QString absolutePath = QFileInfo(rawPath).isAbsolute()
        ? QFileInfo(rawPath).absoluteFilePath()
        : QDir::current().absoluteFilePath(rawPath);

    // Auto-create parent directories
    if (createDirs) {
        const QString parentDir = QFileInfo(absolutePath).absolutePath();
        if (!QDir().mkpath(parentDir)) {
            return QStringLiteral(
                "# Write File Result\n\n"
                "- Path: `%1`\n"
                "- Error: 无法创建父目录 `%2`")
                .arg(QDir::toNativeSeparators(absolutePath),
                     QDir::toNativeSeparators(parentDir));
        }
    }

    const bool existed = QFile::exists(absolutePath);

    QFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral(
            "# Write File Result\n\n"
            "- Path: `%1`\n"
            "- Error: %2")
            .arg(QDir::toNativeSeparators(absolutePath), file.errorString());
    }

    const QByteArray data = content.toUtf8();
    if (file.write(data) != data.size()) {
        return QStringLiteral(
            "# Write File Result\n\n"
            "- Path: `%1`\n"
            "- Error: 写入不完整")
            .arg(QDir::toNativeSeparators(absolutePath));
    }
    file.close();

    return QStringLiteral(
        "# Write File Result\n\n"
        "- Path: `%1`\n"
        "- Size: %2 bytes\n"
        "- Action: %3")
        .arg(QDir::toNativeSeparators(absolutePath))
        .arg(data.size())
        .arg(existed ? QStringLiteral("overwritten") : QStringLiteral("created"));
}

} // namespace LlmTools
