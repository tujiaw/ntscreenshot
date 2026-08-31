#include "SkillManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>

namespace {

constexpr int kSkillFrontmatterReadMaxBytes = 8 * 1024;
constexpr int kSkillNameMaxLength = 64;
constexpr int kSkillDescriptionMaxLength = 1024;
constexpr int kSkillRootSearchMaxDepth = 8;

struct SkillDefinition {
    QString name;
    QString description;
};

QString trimMatchingQuotes(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() >= 2) {
        const QChar first = trimmed.front();
        const QChar last = trimmed.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return trimmed.mid(1, trimmed.size() - 2).trimmed();
        }
    }
    return trimmed;
}

QString frontmatterValue(const QMap<QString, QStringList> &frontmatter, const QString &key)
{
    const QStringList values = frontmatter.value(key);
    if (values.isEmpty()) {
        return QString();
    }
    return trimMatchingQuotes(values.first());
}

QString extractFrontmatterText(const QString &rawContent)
{
    QString content = rawContent;
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (!content.startsWith(QStringLiteral("---\n"))) {
        return QString();
    }

    const int endIndex = content.indexOf(QStringLiteral("\n---\n"), 4);
    if (endIndex <= 0) {
        return QString();
    }

    return content.mid(4, endIndex - 4).trimmed();
}

QMap<QString, QStringList> parseFrontmatter(const QString &frontmatterText)
{
    QMap<QString, QStringList> frontmatter;
    if (frontmatterText.isEmpty()) {
        return frontmatter;
    }

    QString currentKey;
    const QStringList lines = frontmatterText.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        const QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith(QStringLiteral("- ")) && !currentKey.isEmpty()) {
            frontmatter[currentKey].append(trimmedLine.mid(2).trimmed());
            continue;
        }

        const int colonIndex = trimmedLine.indexOf(QLatin1Char(':'));
        if (colonIndex <= 0) {
            continue;
        }

        currentKey = trimmedLine.left(colonIndex).trimmed().toLower();
        const QString value = trimmedLine.mid(colonIndex + 1).trimmed();
        if (!value.isEmpty()) {
            frontmatter[currentKey].append(value);
        } else if (!frontmatter.contains(currentKey)) {
            frontmatter.insert(currentKey, QStringList());
        }
    }

    return frontmatter;
}

bool containsXmlLikeTag(const QString &value)
{
    static const QRegularExpression xmlTagPattern(QStringLiteral("<[^>]+>"));
    return xmlTagPattern.match(value).hasMatch();
}

bool isValidSkillName(const QString &name)
{
    static const QRegularExpression namePattern(QStringLiteral("^[a-z0-9-]+$"));
    if (name.isEmpty() || name.size() > kSkillNameMaxLength) {
        return false;
    }
    if (containsXmlLikeTag(name)) {
        return false;
    }
    if (name.contains(QStringLiteral("anthropic"), Qt::CaseInsensitive)
        || name.contains(QStringLiteral("claude"), Qt::CaseInsensitive)) {
        return false;
    }
    return namePattern.match(name).hasMatch();
}

bool isValidSkillDescription(const QString &description)
{
    if (description.isEmpty() || description.size() > kSkillDescriptionMaxLength) {
        return false;
    }
    return !containsXmlLikeTag(description);
}

SkillDefinition parseSkillDocument(const QString &rawContent)
{
    SkillDefinition skill;

    const QMap<QString, QStringList> frontmatter = parseFrontmatter(extractFrontmatterText(rawContent));
    skill.name = frontmatterValue(frontmatter, QStringLiteral("name"));
    skill.description = frontmatterValue(frontmatter, QStringLiteral("description"));
    return skill;
}

void appendCandidateIfPresent(QStringList &candidates, const QString &path)
{
    if (!path.isEmpty()) {
        candidates.append(QDir::cleanPath(path));
    }
}

void appendSkillRootCandidates(QStringList &candidates, const QDir &baseDir)
{
    static const QStringList kRelativeSkillRoots = {
        QStringLiteral("resource/skills"),
        QStringLiteral("src/resource/skills"),
    };

    for (const QString &relativePath : kRelativeSkillRoots) {
        appendCandidateIfPresent(candidates, baseDir.filePath(relativePath));
    }
}

QStringList candidateSkillRoots()
{
    QStringList candidates;

#ifdef NT_SKILLS_SOURCE_DIR
    appendCandidateIfPresent(candidates, QString::fromUtf8(NT_SKILLS_SOURCE_DIR));
#endif

    // 向上逐级回溯，兼容 VS/CMake 的常见输出目录：
    // build/bin/x64/Debug/ntscreenshot.exe -> ../../../.. -> project root
    QDir cursor(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth <= kSkillRootSearchMaxDepth; ++depth) {
        appendSkillRootCandidates(candidates, cursor);
        if (!cursor.cdUp()) {
            break;
        }
    }

    candidates.removeDuplicates();
    return candidates;
}

QString resolveSkillRoot()
{
    const QStringList candidates = candidateSkillRoots();
    for (const QString &candidate : candidates) {
        QDir dir(candidate);
        if (dir.exists()) {
            return dir.absolutePath();
        }
    }
    return QString();
}

QList<SkillDefinition> loadSkillsFromDirectory(const QString &rootPath)
{
    QList<SkillDefinition> skills;
    if (rootPath.isEmpty()) {
        qDebug() << "[SkillManager] No skill directory resolved";
        return skills;
    }

    QDir rootDir(rootPath);
    const QStringList skillDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &skillDirName : skillDirs) {
        const QString filePath = rootDir.filePath(QStringLiteral("%1/SKILL.md").arg(skillDirName));
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[SkillManager] Failed to open skill file:" << filePath;
            continue;
        }

        const QByteArray rawData = file.read(kSkillFrontmatterReadMaxBytes);
        const SkillDefinition skill = parseSkillDocument(QString::fromUtf8(rawData));
        if (!isValidSkillName(skill.name)) {
            qDebug() << "[SkillManager] Skip invalid skill name:" << filePath << skill.name;
            continue;
        }
        if (!isValidSkillDescription(skill.description)) {
            qDebug() << "[SkillManager] Skip invalid skill description:" << filePath;
            continue;
        }
        skills.append(skill);
    }

    qDebug() << "[SkillManager] Loaded" << skills.size() << "skill(s) from" << rootPath;
    return skills;
}

QString buildPromptText(const QList<SkillDefinition> &allSkills, const QString &skillRoot)
{
    if (allSkills.isEmpty()) {
        return QString();
    }

    Q_UNUSED(skillRoot);
    QStringList sections;
    sections.append(QStringLiteral("<skills>"));
    sections.append(QString());
    for (const SkillDefinition &skill : allSkills) {
        sections.append(QStringLiteral("  <skill>"));
        sections.append(QStringLiteral("    <name>%1</name>").arg(skill.name.toHtmlEscaped()));
        sections.append(QStringLiteral("    <description>%1</description>").arg(skill.description.toHtmlEscaped()));
        sections.append(QStringLiteral("  </skill>"));
        sections.append(QString());
    }
    sections.append(QStringLiteral("</skills>"));

    return sections.join(QStringLiteral("\n"));
}

} // namespace

namespace LlmSkills {

QString SkillManager::buildPrompt(const QString &message)
{
    Q_UNUSED(message);
    const QString skillRoot = resolveSkillRoot();
    const QList<SkillDefinition> skills = loadSkillsFromDirectory(skillRoot);
    const QString prompt = buildPromptText(skills, skillRoot);
    if (prompt.isEmpty()) {
        qDebug() << "[SkillManager] No skills prompt generated";
    } else {
        qDebug() << "[SkillManager] Skills prompt length:" << prompt.length();
    }
    return prompt;
}

QString SkillManager::resolveSkillRoot()
{
    return ::resolveSkillRoot();
}

} // namespace LlmSkills
