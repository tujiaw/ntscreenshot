#pragma once

// Matches portable gitignore-style patterns against root-relative paths.

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

class IgnorePatternMatcher final {
public:
    explicit IgnorePatternMatcher(const QStringList& patterns = {});

    bool isExcluded(const QString& relativePath, bool isDirectory) const;

    static bool validate(const QString& pattern, QString* error = nullptr);

private:
    struct Rule {
        QRegularExpression exactExpression;
        QRegularExpression descendantExpression;
        bool negated = false;
        bool directoryOnly = false;
    };

    static bool compileRule(const QString& pattern, Rule* rule, QString* error);

    QVector<Rule> rules_;
};
