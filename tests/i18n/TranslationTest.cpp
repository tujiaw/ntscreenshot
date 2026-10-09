#include <QCoreApplication>
#include <QTranslator>
#include <utility>
#ifdef Q_OS_WIN
#include "modules/clipboard/HotkeyOptions.h"
#endif

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        return 1;
    }

    // Without an installed translator, the Chinese source text is the fallback.
    if (QCoreApplication::translate("Settings", "常规设置") != QStringLiteral("常规设置")) {
        return 2;
    }
    if (QCoreApplication::translate("App", "背景透明度") != QStringLiteral("背景透明度")) return 8;
#ifdef Q_OS_WIN
    // Initialize the option table before loading a translator, then ensure its
    // labels resolve in the active language rather than caching a translation.
    if (ImageScaleLabel(640) != QStringLiteral("限制在 640 像素内")) return 9;
#endif

    QTranslator english;
    if (!english.load(QString::fromLocal8Bit(argv[1])) || !app.installTranslator(&english)) {
        return 3;
    }
    if (QCoreApplication::translate("Settings", "常规设置") != QStringLiteral("General Settings")) {
        return 4;
    }
    if (QCoreApplication::translate("Settings", "界面语言：") != QStringLiteral("Display language:")) {
        return 5;
    }
    if (QCoreApplication::translate("Settings", "English") != QStringLiteral("English")) {
        return 7;
    }
    if (QCoreApplication::translate("App", "复制") != QStringLiteral("Copy")) {
        return 6;
    }
    const std::pair<const char*, const char*> clipboardLabels[] = {
        {"置顶", "Pin to top"}, {"取消置顶", "Unpin"}, {"删除", "Delete"}, {"AI 填充", "AI Fill"},
        {"预览", "Preview"}, {"无法预览此图片", "Unable to preview this image"},
        {"监控剪切板", "Monitor clipboard"}, {"显示历史记录", "Show history"},
        {"清空历史记录", "Clear history"}, {"唤醒快捷键", "Wake hotkey"},
        {"粘贴快捷键", "Paste hotkey"}, {"图片粘贴尺寸", "Image paste size"},
        {"最大保留条数", "Max retained"}, {"背景透明度", "Background transparency"},
        {"不透明", "Opaque"}, {"AI 填充设置...", "AI Fill Settings..."},
        {"%1 条", "%1 items"}, {"原始尺寸", "Original size"},
        {"限制在 640 像素内", "Fit within 640 px"},
        {"限制在 1024 像素内", "Fit within 1024 px"},
        {"限制在 1600 像素内", "Fit within 1600 px"},
        {"快捷键不可用", "Hotkey unavailable"}, {"粘贴快捷键不可用", "Paste hotkey unavailable"}
    };
    for (const auto& label : clipboardLabels)
        if (QCoreApplication::translate("App", label.first) != QString::fromUtf8(label.second)) return 10;
    if (QCoreApplication::translate("App", "%1 条").arg(20) != QStringLiteral("20 items")) return 11;
#ifdef Q_OS_WIN
    if (ImageScaleLabel(640) != QStringLiteral("Fit within 640 px")) return 12;
#endif
    app.removeTranslator(&english);
    for (const auto& label : clipboardLabels)
        if (QCoreApplication::translate("App", label.first) != QString::fromUtf8(label.first)) return 14;
#ifdef Q_OS_WIN
    if (ImageScaleLabel(640) != QStringLiteral("限制在 640 像素内")) return 13;
#endif
    return 0;
}
