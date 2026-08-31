#include "shared/foundation/IgnorePatternMatcher.h"

#include <QDir>

#include <utility>

namespace {

QString globToRegularExpression(const QString& glob, QString* error)
{
    QString expression;
    expression.reserve(glob.size() * 2);
    for (qsizetype i = 0; i < glob.size(); ++i) {
        const QChar character = glob.at(i);
        if (character == u'*') {
            if (i + 1 < glob.size() && glob.at(i + 1) == u'*') {
                ++i;
                if (i + 1 < glob.size() && glob.at(i + 1) == u'/') {
                    ++i;
                    expression += QStringLiteral("(?:.*/)?");
                } else {
                    expression += QStringLiteral(".*");
                }
            } else {
                expression += QStringLiteral("[^/]*");
            }
        } else if (character == u'?') {
            expression += QStringLiteral("[^/]");
        } else if (character == u'[') {
            const qsizetype closing = glob.indexOf(u']', i + 1);
            if (closing < 0) {
                if (error) *error = QStringLiteral("缺少右方括号 ]");
                return {};
            }
            QString characterClass = glob.mid(i + 1, closing - i - 1);
            if (characterClass.isEmpty()) {
                if (error) *error = QStringLiteral("字符范围不能为空");
                return {};
            }
            if (characterClass.startsWith(u'!')) characterClass[0] = u'^';
            expression += u'[';
            expression += characterClass;
            expression += u']';
            i = closing;
        } else {
            expression += QRegularExpression::escape(QString(character));
        }
    }
    return expression;
}

} // namespace

IgnorePatternMatcher::IgnorePatternMatcher(const QStringList& patterns)
{
    for (const QString& pattern : patterns) {
        Rule rule;
        if (!compileRule(pattern, &rule, nullptr)) continue;
        rules_.push_back(std::move(rule));
    }
}

bool IgnorePatternMatcher::isExcluded(const QString& relativePath, bool isDirectory) const
{
    const QString path = QDir::fromNativeSeparators(relativePath);
    bool excluded = false;
    for (const Rule& rule : rules_) {
        const bool exactMatch = rule.exactExpression.match(path).hasMatch();
        const bool descendantMatch = rule.descendantExpression.match(path).hasMatch();
        if ((exactMatch && (!rule.directoryOnly || isDirectory)) || descendantMatch) {
            excluded = !rule.negated;
        }
    }
    return excluded;
}

bool IgnorePatternMatcher::validate(const QString& pattern, QString* error)
{
    Rule rule;
    return compileRule(pattern, &rule, error);
}

bool IgnorePatternMatcher::compileRule(const QString& source, Rule* rule, QString* error)
{
    QString pattern = source.trimmed();
    if (pattern.isEmpty() || pattern.startsWith(u'#')) return false;

    if (pattern.startsWith(QStringLiteral("\\#"))) {
        pattern.remove(0, 1);
    } else if (pattern.startsWith(u'!')) {
        rule->negated = true;
        pattern.remove(0, 1);
    } else if (pattern.startsWith(QStringLiteral("\\!"))) {
        pattern.remove(0, 1);
    }
    if (pattern.isEmpty()) {
        if (error) *error = QStringLiteral("规则不能为空");
        return false;
    }

    pattern.replace(u'\\', u'/');
    const bool anchored = pattern.startsWith(u'/');
    if (anchored) pattern.remove(0, 1);
    rule->directoryOnly = pattern.endsWith(u'/');
    if (rule->directoryOnly) pattern.chop(1);
    if (pattern.isEmpty()) {
        if (error) *error = QStringLiteral("规则必须包含文件或目录名称");
        return false;
    }

    QString conversionError;
    const QString body = globToRegularExpression(pattern, &conversionError);
    if (!conversionError.isEmpty()) {
        if (error) *error = conversionError;
        return false;
    }
    const bool matchesAtAnyDepth = !anchored && !pattern.contains(u'/');
    const QString prefix = matchesAtAnyDepth ? QStringLiteral("(?:^|/)") : QStringLiteral("^");
    const QRegularExpression::PatternOptions options =
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption;
    rule->exactExpression = QRegularExpression(prefix + body + QStringLiteral("$"), options);
    rule->descendantExpression =
        QRegularExpression(prefix + body + QStringLiteral("/.*$"), options);
    if (!rule->exactExpression.isValid() || !rule->descendantExpression.isValid()) {
        if (error) *error = QStringLiteral("无法解析该规则");
        return false;
    }
    return true;
}
