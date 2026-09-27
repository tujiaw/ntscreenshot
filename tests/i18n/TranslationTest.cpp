#include <QCoreApplication>
#include <QTranslator>

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
    return 0;
}
