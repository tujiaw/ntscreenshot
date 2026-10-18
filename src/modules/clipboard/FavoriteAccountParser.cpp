#include "FavoriteAccountParser.h"
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <algorithm>
#include <iterator>
#include <vector>

namespace {
struct Field {
    int kind;
    qsizetype start;
    qsizetype valueStart;
    qsizetype lineEnd;
    qsizetype quoteEnd;
};

struct UrlOccurrence { qsizetype start; qsizetype end; QString value; };

bool IsWebUrl(const QString& value) {
    if (value.isEmpty() || std::any_of(value.begin(), value.end(), [](QChar c) { return c.isSpace(); })) return false;
    const bool hasScheme = value.contains(QStringLiteral("://"));
    const QUrl url(hasScheme ? value : QStringLiteral("https://") + value, QUrl::StrictMode);
    return url.isValid() && (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
        && !url.host().isEmpty() && (hasScheme || url.host().contains('.') || url.host().contains(':') || url.host() == QStringLiteral("localhost"));
}

std::vector<UrlOccurrence> FindUrls(const QString& text) {
    static const QRegularExpression pattern(QString::fromUtf8(R"rx((?<![\p{L}\p{N}_@])(?:https?://|www\.)[^\s<>"'“”‘’，。；！？、（）【】]+)rx"),
        QRegularExpression::CaseInsensitiveOption);
    std::vector<UrlOccurrence> result;
    auto matches = pattern.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString value = match.captured();
        while (!value.isEmpty()) {
            const auto last = value.back();
            if (QStringLiteral(".,;").contains(last)
                || (last == ')' && value.count(')') > value.count('('))
                || (last == ']' && value.count(']') > value.count('['))
                || (last == '}' && value.count('}') > value.count('{'))) value.chop(1);
            else break;
        }
        result.push_back({match.capturedStart(), match.capturedEnd(), value});
    }
    return result;
}

qsizetype LineEnd(const QString& text, qsizetype start) {
    for (auto i = start; i < text.size(); ++i) {
        const auto c = text[i];
        if (c == '\r' || c == '\n' || c == QChar::LineSeparator || c == QChar::ParagraphSeparator) return i;
    }
    return text.size();
}

qsizetype QuoteEnd(const QString& text, qsizetype start, qsizetype end) {
    if (start >= end) return -1;
    const auto opening = text[start];
    QChar closing;
    if (opening == '"' || opening == '\'') closing = opening;
    else if (opening == QChar(0x201c)) closing = QChar(0x201d);
    else if (opening == QChar(0x2018)) closing = QChar(0x2019);
    else return -1;
    for (auto i = start + 1; i < end; ++i) {
        if (text[i] == '\\') { ++i; continue; }
        if (text[i] == closing) return i;
    }
    return -1;
}
}

FavoriteAccountFields ParseFavoriteAccount(const QString& text) {
    const auto urls = FindUrls(text);
    // Match explicit field labels, not arbitrary prose or URL query parameters.
    // Longer aliases precede shorter aliases. A separator is required after a label.
    static const QRegularExpression labels(QString::fromUtf8(R"rx(
(?:^|[\r\n\x{2028}\x{2029};；,，|｜{]\h*|\h)["'“‘]?
(?:(?<password>登录密码|登陆密码|账户密码|帐户密码|账号密码|帐号密码|用户密码|密\h*码|口令|pass[\h_-]*word|passwd|pwd|pass)
|(?<username>登录用户名|登陆用户名|登录账号|登陆账号|登录帐号|登陆帐号|登录账户|登陆账户|登录用户|登陆用户|登录名|登陆名|用户名|用户名称|用户账号|用户帐号|账户名称|帐户名称|账号名称|帐号名称|账户名|帐户名|账号名|帐号名|用户|账户|帐户|账号|帐号|登录|登陆|邮箱地址|电子邮箱|邮箱|手机号码|手机号|手机|工号|user[\h_-]*name|account[\h_-]*(?:name|id)|login[\h_-]*(?:user[\h_-]*name|name|id)|user[\h_-]*id|email[\h_-]*address|e[\h_-]*mail|username|account|login|user|uid)
|(?<url>登录网址|登陆网址|登录地址|登陆地址|网站地址|网页地址|网址|网站|链接|地址|网页|主页|web[\h_-]*address|web[\h_-]*site|url|site|link|address)
|(?<other>备注|notes?|remark))
["'”’]?\h*(?:[:：=＝]|\t+)
)rx"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption
        | QRegularExpression::ExtendedPatternSyntaxOption);
    std::vector<Field> fields;
    auto matches = labels.globalMatch(text);
    qsizetype quotedUntil = -1;
    qsizetype lineEnd = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (match.capturedStart() < quotedUntil) continue;
        const auto labelStart = match.capturedStart("password") >= 0 ? match.capturedStart("password")
            : match.capturedStart("username") >= 0 ? match.capturedStart("username")
            : match.capturedStart("url") >= 0 ? match.capturedStart("url") : match.capturedStart("other");
        const auto nextUrl = std::upper_bound(urls.begin(), urls.end(), labelStart,
            [](qsizetype position, const UrlOccurrence& url) { return position < url.start; });
        if (nextUrl != urls.begin() && labelStart < std::prev(nextUrl)->end) continue;
        auto start = match.capturedEnd();
        if (start >= lineEnd) lineEnd = LineEnd(text, start);
        const auto end = lineEnd;
        while (start < end && text[start].isSpace()) ++start;
        const auto quote = QuoteEnd(text, start, end);
        if (quote >= 0) quotedUntil = quote + 1;
        fields.push_back({!match.captured("password").isEmpty() ? 1 : !match.captured("username").isEmpty() ? 0
                          : !match.captured("url").isEmpty() ? 2 : 3,
                          match.capturedStart(), start, end, quote});
    }
    QSet<QString> usernames, passwords, taggedUrls, bareUrls;
    std::vector<std::pair<qsizetype, qsizetype>> credentialRanges;
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];
        if (field.kind == 3) continue;
        const auto end = i + 1 < fields.size() ? qMin(field.lineEnd, fields[i + 1].start) : field.lineEnd;
        if (field.kind < 2) credentialRanges.emplace_back(field.valueStart, field.quoteEnd >= 0 ? field.quoteEnd + 1 : end);
        QString value;
        if (field.quoteEnd >= 0) value = text.mid(field.valueStart + 1, field.quoteEnd - field.valueStart - 1);
        else {
            if (end > field.valueStart) value = text.mid(field.valueStart, end - field.valueStart).trimmed();
        }
        if (!value.isEmpty()) {
            if (field.kind == 2) { if (IsWebUrl(value)) taggedUrls.insert(value); }
            else (field.kind == 0 ? usernames : passwords).insert(value);
        }
    }
    for (const auto& url : urls) {
        if (!IsWebUrl(url.value)) continue;
        if (std::none_of(credentialRanges.begin(), credentialRanges.end(), [&](const auto& range) {
                return url.start >= range.first && url.start < range.second;
            })) bareUrls.insert(url.value);
    }
    const auto& recognizedUrls = taggedUrls.isEmpty() ? bareUrls : taggedUrls;
    // Do not silently pick one account when a favorite contains conflicting fields.
    return {usernames.size() == 1 ? *usernames.cbegin() : QString(),
            passwords.size() == 1 ? *passwords.cbegin() : QString(),
            recognizedUrls.size() == 1 ? *recognizedUrls.cbegin() : QString()};
}
