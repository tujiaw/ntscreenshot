#include "core/theme/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QRegularExpression>
#include <QStyleFactory>

#include "core/platform/Util.h"
#include "core/theme/ModernStyle.h"

namespace {

ThemeTokens g_tokens;

QString cssColor(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

QString scaleStyleSheetPixels(const QString& styleSheet)
{
    if (styleSheet.isEmpty()) return styleSheet;
    const double scaleFactor = Util::getScreenScaleFactor();
    if (scaleFactor <= 1.0) return styleSheet;

    QString result;
    result.reserve(styleSheet.size());
    const QRegularExpression pxPattern(QStringLiteral("(\\d+(?:\\.\\d+)?)px"));
    QRegularExpressionMatchIterator matches = pxPattern.globalMatch(styleSheet);
    int lastPosition = 0;
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += styleSheet.mid(lastPosition, match.capturedStart() - lastPosition);
        result += QString::number(qMax(1, qRound(match.captured(1).toDouble() * scaleFactor)));
        result += QStringLiteral("px");
        lastPosition = match.capturedEnd();
    }
    result += styleSheet.mid(lastPosition);
    return result;
}

} // namespace

void ThemeManager::apply(AppTheme theme)
{
    if (!qApp) return;

    g_tokens = createThemeTokens(theme);
    const double scaleFactor = qMax(1.0, Util::getScreenScaleFactor());
    const int radioIndicatorSize = qRound(18 * scaleFactor)
        + 2 * qMax(1, qRound(scaleFactor));
    qApp->setStyle(new ModernStyle(QStyleFactory::create(QStringLiteral("Fusion")),
                                   radioIndicatorSize));

    QPalette palette;
    palette.setColor(QPalette::Window, g_tokens.canvas);
    palette.setColor(QPalette::WindowText, g_tokens.textPrimary);
    palette.setColor(QPalette::Base, g_tokens.surface);
    palette.setColor(QPalette::AlternateBase, g_tokens.surfaceSubtle);
    palette.setColor(QPalette::Text, g_tokens.textPrimary);
    palette.setColor(QPalette::Button, g_tokens.surfaceRaised);
    palette.setColor(QPalette::ButtonText, g_tokens.textPrimary);
    palette.setColor(QPalette::Highlight, g_tokens.accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, g_tokens.accent);
    palette.setColor(QPalette::ToolTipBase, g_tokens.surfaceRaised);
    palette.setColor(QPalette::ToolTipText, g_tokens.textPrimary);
    palette.setColor(QPalette::PlaceholderText, g_tokens.textSecondary);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, g_tokens.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, g_tokens.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, g_tokens.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, g_tokens.surfaceSubtle);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, g_tokens.textDisabled);
    qApp->setPalette(palette);
    qApp->setStyleSheet(qEnvironmentVariableIsSet("NTSCREENSHOT_QA_NO_STYLE")
                            ? QString()
                            : buildStyleSheet(g_tokens));
}

const ThemeTokens& ThemeManager::tokens()
{
    if (!g_tokens.surface.isValid()) {
        g_tokens = createThemeTokens(AppTheme::Dark);
    }
    return g_tokens;
}

QString ThemeManager::webCssVariables()
{
    const ThemeTokens& value = tokens();
    return QStringLiteral(
        ":root{--nt-canvas:%1;--nt-surface:%2;--nt-surface-subtle:%3;"
        "--nt-border:%4;--nt-text:%5;--nt-muted:%6;--nt-accent:%7;"
        "--nt-accent-subtle:%8;--nt-danger:%9;--nt-success:%10;}")
        .arg(cssColor(value.canvas), cssColor(value.surface), cssColor(value.surfaceSubtle),
             cssColor(value.border), cssColor(value.textPrimary), cssColor(value.textSecondary),
             cssColor(value.accent), cssColor(value.accentSubtle), cssColor(value.danger),
             cssColor(value.success));
}

QString ThemeManager::loadStyleSheet()
{
    QFile file(QStringLiteral(":/darkstyle/modernstyle.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString ThemeManager::buildStyleSheet(const ThemeTokens& value)
{
    QString style = loadStyleSheet();
    const QList<QPair<QString, QString>> replacements = {
        {QStringLiteral("@CANVAS@"), cssColor(value.canvas)},
        {QStringLiteral("@SURFACE@"), cssColor(value.surface)},
        {QStringLiteral("@SURFACE_RAISED@"), cssColor(value.surfaceRaised)},
        {QStringLiteral("@SURFACE_SUBTLE@"), cssColor(value.surfaceSubtle)},
        {QStringLiteral("@BORDER@"), cssColor(value.border)},
        {QStringLiteral("@BORDER_STRONG@"), cssColor(value.borderStrong)},
        {QStringLiteral("@TEXT@"), cssColor(value.textPrimary)},
        {QStringLiteral("@TEXT_SECONDARY@"), cssColor(value.textSecondary)},
        {QStringLiteral("@TEXT_DISABLED@"), cssColor(value.textDisabled)},
        {QStringLiteral("@ACCENT@"), cssColor(value.accent)},
        {QStringLiteral("@ACCENT_HOVER@"), cssColor(value.accentHover)},
        {QStringLiteral("@ACCENT_PRESSED@"), cssColor(value.accentPressed)},
        {QStringLiteral("@ACCENT_SUBTLE@"), cssColor(value.accentSubtle)},
        {QStringLiteral("@SUCCESS@"), cssColor(value.success)},
        {QStringLiteral("@DANGER@"), cssColor(value.danger)},
        {QStringLiteral("@WARNING@"), cssColor(value.warning)}
    };
    for (const auto& replacement : replacements) style.replace(replacement.first, replacement.second);
    return scaleStyleSheetPixels(style);
}
