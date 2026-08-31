#include "EditFileTool.h"

#include "LlmToolUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace {

constexpr int kMaxEditFileSize = 256 * 1024; // 256KB

} // namespace

namespace LlmTools {

QString EditFileTool::name() const
{
    return QStringLiteral("edit_file");
}

QString EditFileTool::description() const
{
    return QStringLiteral("精确替换文件中的文本片段。适合修改代码、编辑配置。每次调用只替换第一个匹配。");
}

QJsonObject EditFileTool::parameters() const
{
    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), QJsonObject{
        {QStringLiteral("path"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要编辑的文件路径")}
        }},
        {QStringLiteral("old_text"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要被替换的原始文本（必须精确匹配）")}
        }},
        {QStringLiteral("new_text"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("替换后的新文本")}
        }},
        {QStringLiteral("replace_all"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("boolean")},
            {QStringLiteral("description"), QStringLiteral("是否替换所有匹配项，默认 false（只替换第一个）")}
        }}
    });
    parametersObject.insert(QStringLiteral("required"), QJsonArray{
        QStringLiteral("path"), QStringLiteral("old_text"), QStringLiteral("new_text")});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString EditFileTool::execute(const QJsonObject &arguments) const
{
    const QString rawPath = arguments.value(QStringLiteral("path")).toString().trimmed();
    const QString oldText = arguments.value(QStringLiteral("old_text")).toString();
    const QString newText = arguments.value(QStringLiteral("new_text")).toString();
    const bool replaceAll = arguments.value(QStringLiteral("replace_all")).toBool(false);

    if (rawPath.isEmpty()) {
        return argumentError(QStringLiteral("缺少 path 参数"));
    }
    if (!arguments.contains(QStringLiteral("old_text"))) {
        return argumentError(QStringLiteral("缺少 old_text 参数"));
    }
    if (!arguments.contains(QStringLiteral("new_text"))) {
        return argumentError(QStringLiteral("缺少 new_text 参数"));
    }

    const QString absolutePath = QFileInfo(rawPath).isAbsolute()
        ? QFileInfo(rawPath).absoluteFilePath()
        : QDir::current().absoluteFilePath(rawPath);

    QFile file(absolutePath);
    if (!file.exists()) {
        return QStringLiteral(
            "# Edit File Result\n\n"
            "- Path: `%1`\n"
            "- Error: 文件不存在")
            .arg(QDir::toNativeSeparators(absolutePath));
    }

    if (file.size() > kMaxEditFileSize) {
        return QStringLiteral(
            "# Edit File Result\n\n"
            "- Path: `%1`\n"
            "- Error: 文件过大（超过 %2 字节），不支持编辑")
            .arg(QDir::toNativeSeparators(absolutePath))
            .arg(kMaxEditFileSize);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral(
            "# Edit File Result\n\n"
            "- Path: `%1`\n"
            "- Error: %2")
            .arg(QDir::toNativeSeparators(absolutePath), file.errorString());
    }

    QString content = normalizeLineEndings(QString::fromUtf8(file.readAll()));
    file.close();

    if (!content.contains(oldText)) {
        return QStringLiteral(
            "# Edit File Result\n\n"
            "- Path: `%1`\n"
            "- Error: 未找到匹配的 old_text\n"
            "- Hint: 请确保 old_text 与文件中的内容完全一致（包括空白和换行）")
            .arg(QDir::toNativeSeparators(absolutePath));
    }

    int replaceCount = 0;
    if (replaceAll) {
        replaceCount = content.count(oldText);
        content.replace(oldText, newText);
    } else {
        replaceCount = 1;
        content.replace(content.indexOf(oldText), oldText.size(), newText);
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral(
            "# Edit File Result\n\n"
            "- Path: `%1`\n"
            "- Error: 无法写入文件：%2")
            .arg(QDir::toNativeSeparators(absolutePath), file.errorString());
    }

    const QByteArray data = content.toUtf8();
    file.write(data);
    file.close();

    return QStringLiteral(
        "# Edit File Result\n\n"
        "- Path: `%1`\n"
        "- Replacements: %2\n"
        "- File size: %3 bytes")
        .arg(QDir::toNativeSeparators(absolutePath))
        .arg(replaceCount)
        .arg(data.size());
}

} // namespace LlmTools
