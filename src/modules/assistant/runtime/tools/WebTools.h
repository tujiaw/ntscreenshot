#pragma once
#include "LlmTool.h"
#include "BackgroundBrowser.h"
#include "ToolAbort.h"
#include <QJsonArray>
#include <QString>
#include <QUrl>

namespace LlmTools {

class RunJavaScriptTool final : public LlmTool {
public:
    explicit RunJavaScriptTool(BackgroundBrowser *browser) : browser_(browser) {}
    QString name() const override { return QStringLiteral("run_javascript"); }
    QString description() const override {
        return QStringLiteral("在指定网页上执行任意 JavaScript 并返回结果。可用于点击按钮、填写表单、滚动页面，或抓取页面中任意元素/表格/结构化数据。脚本运行在页面上下文，`return` 的值会被 JSON 序列化后返回。");
    }
    QJsonObject parameters() const override {
        return {{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("url"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("script"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("url"), QStringLiteral("script")}},
                {QStringLiteral("additionalProperties"), false}};
    }
    QString execute(const QJsonObject &args) const override { return execute(args, nullptr); }
    QString execute(const QJsonObject &args, ToolAbort *abort) const override {
        const QString url = args.value(QStringLiteral("url")).toString().trimmed();
        const QString script = args.value(QStringLiteral("script")).toString();
        if (url.isEmpty()) return argumentError(QStringLiteral("url 不能为空"));
        if (script.trimmed().isEmpty()) return argumentError(QStringLiteral("script 不能为空"));
        return browser_->evaluate(QUrl(url), script, abort);
    }
private:
    BackgroundBrowser *browser_;
};

class ReadArticleTool final : public LlmTool {
public:
    explicit ReadArticleTool(BackgroundBrowser *browser) : browser_(browser) {}
    QString name() const override { return QStringLiteral("read_article"); }
    QString description() const override {
        return QStringLiteral("加载网页并提取正文主体内容，自动去掉导航、广告、侧栏等噪音，返回标题与干净的正文文本，适合阅读文章或新闻。");
    }
    QJsonObject parameters() const override {
        return {{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("url"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                }},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("url")}},
                {QStringLiteral("additionalProperties"), false}};
    }
    QString execute(const QJsonObject &args) const override { return execute(args, nullptr); }
    QString execute(const QJsonObject &args, ToolAbort *abort) const override {
        const QString url = args.value(QStringLiteral("url")).toString().trimmed();
        if (url.isEmpty()) return argumentError(QStringLiteral("url 不能为空"));
        return browser_->readArticle(QUrl(url), abort);
    }
private:
    BackgroundBrowser *browser_;
};

} // namespace LlmTools
