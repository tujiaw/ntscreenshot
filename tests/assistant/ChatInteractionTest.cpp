#include "modules/assistant/ui/ChatInputWidget.h"
#include "core/theme/ThemeManager.h"
#include "core/settings/SettingModel.h"
#include <QTemporaryDir>
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QToolButton>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTimer>
#include <cstdio>

// Deterministic DPI and no access to user settings in this widget test.
static QString configPath;
namespace Util {
int scaleSize(int size) { return size; }
double getScreenScaleFactor() { return 1.0; }
QString getConfigPath() { return configPath; }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    configPath = directory.filePath("settings.ini");
    QSettings initial(configPath, QSettings::IniFormat);
    initial.setValue("test", true); initial.sync();
    SettingModel settings(nullptr);
    bool failed = false;
    const auto check = [&](bool okay, const char *message) {
        if (!okay) { std::fprintf(stderr, "%s\n", message); failed = true; }
    };
    for (auto theme : {AppTheme::Light, AppTheme::Dark}) {
        ThemeManager::apply(theme);
        ChatInputWidget input(&settings);
        for (bool browser : {false, true}) {
            input.setPending(false);
            if (browser) input.setBrowserEnabled(true); else input.setWebEnabled(true);
            const auto buttons = input.findChildren<QToolButton *>(QStringLiteral("chatInputToggleButton"));
            QToolButton *selected = nullptr;
            for (auto *button : buttons) if (button->isChecked()) selected = button;
            check(selected != nullptr, "mode must be selected");
            if (!selected) continue;
            const auto normal = selected->icon().pixmap(selected->iconSize(), QIcon::Normal, QIcon::On).toImage();
            input.setPending(true);
            const auto disabled = selected->icon().pixmap(selected->iconSize(), QIcon::Disabled, QIcon::On).toImage();
            check(!normal.isNull() && normal == disabled, "pending mode icon must retain its selected color");
            selected->click();
            check(selected->isChecked() && !selected->isEnabled(), "mode must stay locked and selected while pending");
            input.setPending(false);
            check(selected->isChecked() && selected->isEnabled(), "mode must survive request completion");
        }
    }

    QWebEngineView view;
    view.resize(800, 600);
    QFile style(QStringLiteral(CHAT_HTML_DIR) + "/chat-tool.css");
    if (!style.open(QIODevice::ReadOnly)) return 1;
    QString html = "<html><head><style>" + QString::fromUtf8(style.readAll())
        + "</style></head><body><div id='message-list'></div>";
    for (const QString &name : {QStringLiteral("chat-core.js"), QStringLiteral("marked.umd.js"),
         QStringLiteral("chat-markdown.js"), QStringLiteral("chat-actions.js"), QStringLiteral("chat-renderer.js")}) {
        QFile file(QStringLiteral(CHAT_HTML_DIR) + "/" + name);
        if (!file.open(QIODevice::ReadOnly)) return 1;
        html += "<script>" + QString::fromUtf8(file.readAll()) + "</script>";
    }
    html += "</body></html>";
    QEventLoop loaded;
    QObject::connect(&view, &QWebEngineView::loadFinished, &loaded, &QEventLoop::quit);
    QTimer::singleShot(10000, &loaded, &QEventLoop::quit);
    view.setHtml(html);
    view.show();
    loaded.exec();
    const QString script = QStringLiteral(R"JS((function() {
        const r = ChatApp.renderer;
        ChatApp.state.messageList = document.getElementById('message-list');
        r.toolActivity({action:'executing',name:'read',args:'{}'});
        r.toolActivity({action:'executed',name:'read',result:'原文\n'.repeat(500)});
        document.querySelector('.chat-tool-card-toggle').click();
        const trigger = document.querySelector('.chat-tool-card-full-result');
        trigger.style.cssText = 'position:fixed;left:100px;top:350px;height:20px;display:block';
        trigger.dispatchEvent(new MouseEvent('mouseenter'));
        const tip = document.querySelector('.chat-tool-result-tooltip');
        const a = trigger.getBoundingClientRect(), b = tip.getBoundingClientRect();
        if (b.top < a.bottom && b.bottom > a.top) return 'tooltip overlaps trigger';
        trigger.dispatchEvent(new MouseEvent('mouseleave',{relatedTarget:tip.querySelector('pre')}));
        if (!tip.isConnected) return 'entering tooltip content closes tooltip';
        trigger.dispatchEvent(new Event('focus'));
        if (document.querySelector('.chat-tool-result-tooltip') !== tip) return 'focus rebuilds open tooltip';
        r.appendMessage({id:'stream',role:'assistant',text:'streaming',completed:false});
        r.updateMessage({id:'stream',role:'assistant',text:'streaming more',completed:true});
        if (document.querySelector('.chat-tool-result-tooltip') !== tip) return 'streaming replaced tooltip';
        tip.dispatchEvent(new MouseEvent('mouseleave',{relatedTarget:document.body}));
        if (tip.isConnected) return 'leaving tooltip does not close it';
        return 'ok';
    })())JS");
    QEventLoop evaluated;
    QString result;
    view.page()->runJavaScript(script, [&](const QVariant &value) { result = value.toString(); evaluated.quit(); });
    QTimer::singleShot(10000, &evaluated, &QEventLoop::quit);
    evaluated.exec();
    check(result == "ok", qPrintable(result.isEmpty() ? QStringLiteral("renderer test did not complete") : result));
    return failed ? 1 : 0;
}
