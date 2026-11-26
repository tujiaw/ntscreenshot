#include "Settings.h"
#include "SettingsInternal.h"

#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QVariant>

#include "app/WindowManager.h"
#include "core/platform/CaptureCursor.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/DarkStyle.h"
#include "core/theme/ThemeManager.h"

namespace {
QPixmap cursorPreviewPixmap(ScreenshotCursorStyle style)
{
    const QPixmap custom = Util::screenshotCursor(style).pixmap();
    if (!custom.isNull()) return custom;
#ifdef Q_OS_WIN
    // Standard QCursor shapes have no Qt pixmap. Read the native cursor
    // instead of substituting our custom black-and-white crosshair.
    const HCURSOR native = LoadCursorW(nullptr, IDC_CROSS);
    if (native) {
        // Monochrome system cursors can use XOR pixels, which disappear when
        // converted directly to an alpha icon. Composite through Windows first.
        QImage backing(GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR), QImage::Format_RGB32);
        const QColor background = ThemeManager::tokens().surfaceSubtle;
        backing.fill(background);
        const HBITMAP bitmap = backing.toHBITMAP();
        const HDC dc = CreateCompatibleDC(nullptr);
        if (bitmap && dc) {
            const HGDIOBJ previous = SelectObject(dc, bitmap);
            const bool drawn = DrawIconEx(dc, 0, 0, reinterpret_cast<HICON>(native),
                                           backing.width(), backing.height(), 0, nullptr, DI_NORMAL);
            SelectObject(dc, previous);
            if (drawn) {
                QImage rendered = QImage::fromHBITMAP(bitmap).convertToFormat(QImage::Format_RGB32)
                    .convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < rendered.height(); ++y)
                    for (int x = 0; x < rendered.width(); ++x)
                        if (rendered.pixelColor(x, y) == background) rendered.setPixel(x, y, 0);
                DeleteDC(dc);
                DeleteObject(bitmap);
                return QPixmap::fromImage(rendered);
            }
        }
        if (dc) DeleteDC(dc);
        if (bitmap) DeleteObject(bitmap);
    }
#endif
    // A simple uninterrupted cross is the generic system-shape fallback.
    const int size = Util::scaleSize(32);
    QPixmap preview(size, size);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    painter.setPen(QPen(Qt::black, qMax(1, Util::scaleSize(1))));
    const int center = size / 2;
    const int margin = Util::scaleSize(6);
    painter.drawLine(center, margin, center, size - margin - 1);
    painter.drawLine(margin, center, size - margin - 1, center);
    return preview;
}

}

void Settings::initThemeSelector()
{
    QHBoxLayout *themeLayout = new QHBoxLayout();
    themeLayout->setObjectName(QStringLiteral("generalThemeRow"));
    QLabel *themeLabel = new QLabel(QCoreApplication::translate("App", "皮肤:"), this);
    cbTheme_ = new QComboBox(this);
    cbTheme_->addItem(QCoreApplication::translate("App", "黑色"), static_cast<int>(AppTheme::Dark));
    cbTheme_->addItem(QCoreApplication::translate("App", "白色"), static_cast<int>(AppTheme::Light));
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(cbTheme_);
    themeLayout->addStretch();

    ui.verticalLayout_2->insertLayout(1, themeLayout);

    connect(cbTheme_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Settings::onThemeChanged);
}

void Settings::initCursorSelector()
{
    auto* row = new QHBoxLayout();
    row->setObjectName(QStringLiteral("generalCursorRow"));
    auto* label = new QLabel(QCoreApplication::translate("App", "截图光标"), this);
    cbCursor_ = new QComboBox(this);
    cbCursor_->setObjectName(QStringLiteral("screenshotCursorSelector"));
    cbCursor_->setMinimumWidth(Util::scaleSize(160));
    const QList<QPair<ScreenshotCursorStyle, QString>> options = {
        {ScreenshotCursorStyle::Color, QCoreApplication::translate("App", "鲜彩十字")},
        {ScreenshotCursorStyle::Classic, QCoreApplication::translate("App", "经典彩色")},
        {ScreenshotCursorStyle::Contrast, QCoreApplication::translate("App", "黑白十字")},
        {ScreenshotCursorStyle::System, QCoreApplication::translate("App", "系统十字")}
    };
    for (const auto& option : options) {
        const QPixmap preview = cursorPreviewPixmap(option.first);
        cbCursor_->addItem(QIcon(preview), option.second, static_cast<int>(option.first));
    }
    cursorPreview_ = new QLabel(this);
    cursorPreview_->setObjectName(QStringLiteral("screenshotCursorPreview"));
    cursorPreview_->setFixedSize(Util::scaleSize(64), Util::scaleSize(48));
    cursorPreview_->setAlignment(Qt::AlignCenter);
    auto* hint = new QLabel(QCoreApplication::translate("App", "移到预览上查看"), this);
    hint->setWordWrap(true);
    row->addWidget(label);
    row->addWidget(cbCursor_);
    row->addWidget(cursorPreview_);
    row->addWidget(hint);
    row->addStretch();
    ui.verticalLayout_2->insertLayout(2, row);
    connect(cbCursor_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        windowManager_->setting()->setScreenshotCursorStyle(validatedCursorStyle(cbCursor_->itemData(index).toInt()));
        refreshCursorPreview();
    });
}

void Settings::refreshCursorPreview()
{
    if (!cbCursor_ || !cursorPreview_) return;
    const auto style = validatedCursorStyle(cbCursor_->currentData().toInt());
    const QCursor cursor = Util::screenshotCursor(style);
    cursorPreview_->setCursor(cursor);
    const QPixmap preview = cursorPreviewPixmap(style);
    cursorPreview_->setPixmap(preview.scaled(cursorPreview_->size() - QSize(Util::scaleSize(8), Util::scaleSize(8)),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Settings::initSelectionBorderColor()
{
    auto* row = new QHBoxLayout();
    row->setObjectName(QStringLiteral("generalBorderColorRow"));
    row->addWidget(new QLabel(QCoreApplication::translate("App", "框选边框颜色"), this));
    selectionBorderColorButton_ = new QPushButton(this);
    selectionBorderColorButton_->setObjectName(QStringLiteral("selectionBorderColorButton"));
    selectionBorderColorButton_->setMinimumWidth(Util::scaleSize(140));
    selectionBorderColorButton_->setToolTip(QCoreApplication::translate("App", "选择框选边框颜色"));
    row->addWidget(selectionBorderColorButton_);
    auto* reset = new QPushButton(QCoreApplication::translate("App", "恢复默认"), this);
    reset->setObjectName(QStringLiteral("resetSelectionBorderColorButton"));
    row->addWidget(reset);
    row->addStretch();
    ui.verticalLayout_2->insertLayout(3, row);
    auto* styleRow = new QHBoxLayout();
    styleRow->setObjectName(QStringLiteral("generalBorderStyleRow"));
    styleRow->addWidget(new QLabel(QCoreApplication::translate("App", "框选边框样式"), this));
    selectionBorderStyleCombo_ = new QComboBox(this);
    selectionBorderStyleCombo_->setObjectName(QStringLiteral("selectionBorderStyleCombo"));
    selectionBorderStyleCombo_->setMinimumWidth(Util::scaleSize(140));
    selectionBorderStyleCombo_->addItem(QCoreApplication::translate("App", "实线"), int(Qt::SolidLine));
    selectionBorderStyleCombo_->addItem(QCoreApplication::translate("App", "虚线"), int(Qt::DashLine));
    selectionBorderStyleCombo_->addItem(QCoreApplication::translate("App", "点线"), int(Qt::DotLine));
    selectionBorderStyleCombo_->addItem(QCoreApplication::translate("App", "点划线"), int(Qt::DashDotLine));
    selectionBorderStyleCombo_->addItem(QCoreApplication::translate("App", "双点划线"), int(Qt::DashDotDotLine));
    styleRow->addWidget(selectionBorderStyleCombo_);
    auto* resetStyle = new QPushButton(QCoreApplication::translate("App", "恢复默认"), this);
    styleRow->addWidget(resetStyle);
    styleRow->addStretch();
    ui.verticalLayout_2->insertLayout(4, styleRow);
    connect(selectionBorderStyleCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) return;
        windowManager_->setting()->setSelectionBorderStyle(
            static_cast<Qt::PenStyle>(selectionBorderStyleCombo_->itemData(index).toInt()));
    });
    connect(resetStyle, &QPushButton::clicked, this, [this] {
        selectionBorderStyleCombo_->setCurrentIndex(selectionBorderStyleCombo_->findData(int(Qt::SolidLine)));
    });
    connect(selectionBorderColorButton_, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(windowManager_->setting()->selectionBorderColor(), this,
            QCoreApplication::translate("App", "选择框选边框颜色"));
        if (!color.isValid()) return;
        windowManager_->setting()->setSelectionBorderColor(color);
        refreshSelectionBorderColor();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        windowManager_->setting()->setSelectionBorderColor(Style::DRAW_SELECTED_BORDER_COLOR);
        refreshSelectionBorderColor();
    });
}

void Settings::refreshSelectionBorderColor()
{
    if (selectionBorderStyleCombo_) {
        const QSignalBlocker blocker(selectionBorderStyleCombo_);
        selectionBorderStyleCombo_->setCurrentIndex(selectionBorderStyleCombo_->findData(
            int(windowManager_->setting()->selectionBorderStyle())));
    }
    if (!selectionBorderColorButton_) return;
    const QColor color = windowManager_->setting()->selectionBorderColor();
    QPixmap swatch(Util::scaleSize(18), Util::scaleSize(18));
    swatch.fill(color);
    selectionBorderColorButton_->setIcon(QIcon(swatch));
    selectionBorderColorButton_->setText(color.name(QColor::HexRgb).toUpper());
}

void Settings::initAmplifierCrossColor()
{
    auto* row = new QHBoxLayout();
    row->setObjectName(QStringLiteral("generalCrossColorRow"));
    row->addWidget(new QLabel(QCoreApplication::translate("App", "放大器十字颜色"), this));
    amplifierCrossColorButton_ = new QPushButton(this);
    amplifierCrossColorButton_->setObjectName(QStringLiteral("amplifierCrossColorButton"));
    amplifierCrossColorButton_->setMinimumWidth(Util::scaleSize(140));
    amplifierCrossColorButton_->setToolTip(QCoreApplication::translate("App", "选择放大器十字颜色"));
    row->addWidget(amplifierCrossColorButton_);
    auto* reset = new QPushButton(QCoreApplication::translate("App", "恢复默认"), this);
    reset->setObjectName(QStringLiteral("resetAmplifierCrossColorButton"));
    row->addWidget(reset);
    row->addStretch();
    ui.verticalLayout_2->insertLayout(5, row);
    connect(amplifierCrossColorButton_, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(windowManager_->setting()->amplifierCrossColor(), this,
            QCoreApplication::translate("App", "选择放大器十字颜色"));
        if (!color.isValid()) return;
        windowManager_->setting()->setAmplifierCrossColor(color);
        refreshAmplifierCrossColor();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        windowManager_->setting()->setAmplifierCrossColor(Style::AMPLIFIER_CROSS_COLOR);
        refreshAmplifierCrossColor();
    });
}

void Settings::refreshAmplifierCrossColor()
{
    if (!amplifierCrossColorButton_) return;
    const QColor color = windowManager_->setting()->amplifierCrossColor();
    QPixmap swatch(Util::scaleSize(18), Util::scaleSize(18));
    swatch.fill(QColor(color.name(QColor::HexRgb)));
    amplifierCrossColorButton_->setIcon(QIcon(swatch));
    amplifierCrossColorButton_->setText(color.name(QColor::HexRgb).toUpper());
}


void Settings::onRgbColorToggled(bool checked)
{
	windowManager_->setting()->setRgbColor(checked);
}

void Settings::onHexColorToggled(bool checked)
{
	windowManager_->setting()->setRgbColor(!checked);
}


void Settings::onBackgroundChanged()
{
    ui.hsBackgroundAlpha->setEnabled(ui.cbBackground->isChecked());
    if (!ui.cbBackground->isChecked()) {
        ui.hsBackgroundAlpha->setValue(160);
    }
    windowManager_->setting()->setBackgroundColor(ui.cbBackground->isChecked(), ui.hsBackgroundAlpha->value());
    showStatusTip(QCoreApplication::translate("App", "截图背景设置已更新"));
}

void Settings::onBackgroundAlphaReleased()
{
    windowManager_->setting()->setBackgroundColor(ui.cbBackground->isChecked(), ui.hsBackgroundAlpha->value());
    showStatusTip(QCoreApplication::translate("App", "背景透明度已更新"));
}


void Settings::onThemeChanged(int index)
{
    if (!cbTheme_ || index < 0) {
        return;
    }

    const QVariant data = cbTheme_->itemData(index);
    if (!data.isValid()) {
        return;
    }

    const AppTheme themeMode = static_cast<AppTheme>(data.toInt());
    SettingModel *setting = windowManager_->setting();
    if (setting->themeMode() == themeMode) {
        return;
    }

    setting->setThemeMode(themeMode);
    CDarkStyle::assign(themeMode);
    emit windowManager_->sigSettingChanged();
    showStatusTip(themeMode == AppTheme::Dark ? QCoreApplication::translate("App", "已切换为黑色主题") : QCoreApplication::translate("App", "已切换为白色主题"));
}
