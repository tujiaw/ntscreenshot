#include "ListDirectoryTool.h"

#include "LlmToolUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonArray>

namespace {

constexpr int kDefaultMaxEntries = 100;
constexpr int kMaxMaxEntries = 500;

QString formatEntry(const QFileInfo &info)
{
    const QString type = info.isDir()
        ? QStringLiteral("DIR")
        : QStringLiteral("FILE");
    const QString size = info.isFile()
        ? QStringLiteral("%1 B").arg(info.size())
        : QStringLiteral("-");
    const QString modified = info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    return QStringLiteral("%1  %2  %3  %4")
        .arg(type, -4)
        .arg(size, -12)
        .arg(modified)
        .arg(info.fileName());
}

} // namespace

namespace LlmTools {

QString ListDirectoryTool::name() const
{
    return QStringLiteral("ls");
}

QString ListDirectoryTool::description() const
{
    return QStringLiteral("列出目录内容，显示文件和子目录的信息。适合浏览项目结构、查找文件。");
}

QJsonObject ListDirectoryTool::parameters() const
{
    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), QJsonObject{
        {QStringLiteral("path"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要列出的目录路径，默认为当前工作目录")}
        }},
        {QStringLiteral("max_entries"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"), QStringLiteral("最大返回条目数，默认 100")}
        }},
        {QStringLiteral("recursive"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("boolean")},
            {QStringLiteral("description"), QStringLiteral("是否递归列出子目录，默认 false")}
        }}
    });
    parametersObject.insert(QStringLiteral("required"), QJsonArray{});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString ListDirectoryTool::execute(const QJsonObject &arguments) const
{
    const QString rawPath = arguments.value(QStringLiteral("path")).toString().trimmed();
    const int maxEntries = clampInt(
        arguments.value(QStringLiteral("max_entries")).toInt(kDefaultMaxEntries),
        1, kMaxMaxEntries);
    const bool recursive = arguments.value(QStringLiteral("recursive")).toBool(false);

    const QString dirPath = rawPath.isEmpty()
        ? QDir::currentPath()
        : (QFileInfo(rawPath).isAbsolute()
            ? QFileInfo(rawPath).absoluteFilePath()
            : QDir::current().absoluteFilePath(rawPath));

    const QDir dir(dirPath);
    if (!dir.exists()) {
        return QStringLiteral(
            "# List Directory Result\n\n"
            "- Path: `%1`\n"
            "- Error: 目录不存在")
            .arg(QDir::toNativeSeparators(dirPath));
    }

    const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    const QDir::SortFlags sort = QDir::DirsFirst | QDir::Name | QDir::IgnoreCase;

    QStringList entries;
    if (recursive) {
        const QStringList rawEntries = recursiveList(dir, dirPath, filters, sort, maxEntries);
        for (const QString &entry : rawEntries) {
            entries.append(entry);
        }
    } else {
        const QFileInfoList infoList = dir.entryInfoList(filters, sort);
        int count = 0;
        for (const QFileInfo &info : infoList) {
            if (count >= maxEntries) break;
            entries.append(formatEntry(info));
            ++count;
        }
    }

    const bool truncated = entries.size() >= maxEntries;
    if (truncated) {
        entries.append(QStringLiteral("... [结果已截断，共显示 %1 条]").arg(entries.size()));
    }

    const QString result = QStringLiteral(
        "# List Directory Result\n\n"
        "- Path: `%1`\n"
        "- Entries: %2\n\n"
        "```\n%3\n```")
        .arg(QDir::toNativeSeparators(dirPath))
        .arg(truncated ? QStringLiteral(">=%1").arg(maxEntries) : QString::number(entries.size()))
        .arg(entries.join(QLatin1Char('\n')));
    return truncateText(result, kToolOutputMaxChars);
}

QStringList ListDirectoryTool::recursiveList(const QDir &dir, const QString &basePath,
                                              QDir::Filters filters, QDir::SortFlags sort,
                                              int maxEntries) const
{
    QStringList result;
    const QFileInfoList infoList = dir.entryInfoList(filters, sort);

    for (const QFileInfo &info : infoList) {
        if (result.size() >= maxEntries) break;

        const QString relative = QDir(basePath).relativeFilePath(info.absoluteFilePath());
        if (info.isDir()) {
            result.append(QStringLiteral("DIR   %1/").arg(relative));
        } else {
            const QString size = QStringLiteral("%1 B").arg(info.size());
            result.append(QStringLiteral("FILE  %1  %2").arg(relative, -40).arg(size));
        }
    }

    // Recurse into subdirectories
    const QFileInfoList subDirs = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Dirs, sort);
    for (const QFileInfo &subDir : subDirs) {
        if (result.size() >= maxEntries) break;
        if (!subDir.isDir()) continue;

        QDir sub(subDir.absoluteFilePath());
        const QStringList subEntries = recursiveList(sub, basePath, filters, sort, maxEntries - result.size());
        result.append(subEntries);
    }

    return result;
}

} // namespace LlmTools
