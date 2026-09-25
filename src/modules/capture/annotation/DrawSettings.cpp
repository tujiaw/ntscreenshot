#include "DrawSettings.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QPainter>
#include "core/platform/Util.h"
#include "core/theme/ThemeManager.h"

static int s_penWidth = 3;
static int s_fontSize = 16;
static QColor s_currentColor = QColor("#FF0000");

namespace {

// 线宽预览按钮：用水平线条展示真实的笔画粗细，选中态为白底+阴影
class PenWidthButton : public QPushButton
{
public:
    explicit PenWidthButton(int value, QWidget *parent = nullptr)
        : QPushButton(parent), value_(value)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setProperty("value", value);
        setToolTip(QStringLiteral("线宽 %1").arg(value));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const ThemeTokens& tokens = ThemeManager::tokens();
        const bool checked = isChecked();
        const bool hovered = underMouse();

        if (checked) {
            // 选中态：白底 + 微妙阴影
            p.setPen(Qt::NoPen);
            p.setBrush(tokens.surfaceRaised);
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1),
                              Util::scaleSize(5), Util::scaleSize(5));
        } else if (hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(tokens.surfaceSubtle);
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1),
                              Util::scaleSize(5), Util::scaleSize(5));
        }

        const qreal y = rect().center().y() + 0.5;
        const qreal w = qMax<qreal>(1.0, Util::scaleSize(value_));
        p.setPen(QPen(checked ? tokens.accent : tokens.textSecondary, w,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(rect().left() + Util::scaleSize(6), y),
                   QPointF(rect().right() - Util::scaleSize(6), y));
    }

    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override { update(); }

private:
    int value_;
};

// 颜色块按钮：圆形色块，选中时显示双重圆环（白色内环 + 强调色外环）
class ColorSwatchButton : public QPushButton
{
public:
    explicit ColorSwatchButton(const QColor &color, QWidget *parent = nullptr)
        : QPushButton(parent), color_(color)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setProperty("color", color.name(QColor::HexRgb));
        setToolTip(color.name(QColor::HexRgb).toUpper());
    }

    QColor color() const { return color_; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const ThemeTokens& tokens = ThemeManager::tokens();

        const qreal margin = Util::scaleSize(1);
        const QRectF r = QRectF(rect()).adjusted(margin, margin, -margin, -margin);

        if (isChecked()) {
            // 外环：强调色
            p.setPen(QPen(tokens.accent, qMax(1, Util::scaleSize(2))));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(r);
            // 内环：面板背景色（形成双重圆环效果）
            const qreal innerMargin = Util::scaleSize(2);
            const QRectF inner = r.adjusted(innerMargin, innerMargin, -innerMargin, -innerMargin);
            p.setPen(QPen(tokens.surface, qMax(1, Util::scaleSize(1))));
            p.drawEllipse(inner);
            // 填充色块
            const qreal fillMargin = Util::scaleSize(3);
            const QRectF fill = r.adjusted(fillMargin, fillMargin, -fillMargin, -fillMargin);
            p.setPen(Qt::NoPen);
            p.setBrush(color_);
            p.drawEllipse(fill);
        } else {
            // 默认态：浅描边 + 填充
            p.setPen(QPen(QColor(0, 0, 0, 38), qMax(1, Util::scaleSize(1))));
            p.setBrush(color_);
            p.drawEllipse(r);

            if (underMouse()) {
                p.setPen(QPen(tokens.accent, qMax(1, Util::scaleSize(1))));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(r);
            }
        }
    }

    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override { update(); }

private:
    QColor color_;
};

} // namespace

DrawSettings::DrawSettings(QWidget *parent)
    : QWidget(parent)
{
    this->setObjectName(QStringLiteral("DrawSettingsPanel"));
    this->setAutoFillBackground(true);

    auto makeLabel = [this](const QString &text) -> QLabel* {
        QLabel *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("DrawSettingsLabel"));
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return label;
    };

    // 笔宽：三条线条预览（真实粗细）
    const QList<int> penWidthList = { 1, 3, 6 };
    for (int w : penWidthList) {
        auto *btn = new PenWidthButton(w, this);
        connect(btn, &QPushButton::clicked, this, &DrawSettings::onPenWidthSelected);
        btn->setChecked(w == s_penWidth);
        penWidthBtns_.push_back(btn);
    }

    // 字号
    sizeList_ = new QComboBox(this);
    sizeList_->addItems(QStringList()
                        << "8" << "9" << "10" << "11" << "12" << "14" << "16" << "18" << "20" << "22");
    sizeList_->setCurrentText(QString::number(s_fontSize));
    sizeList_->setToolTip(QStringLiteral("字号"));
    connect(sizeList_, &QComboBox::currentTextChanged, this, &DrawSettings::onFontSizeChanged);

    // 当前颜色（点击打开自定义颜色对话框）
    pbCurrentColor_ = new QPushButton(this);
    pbCurrentColor_->setCursor(Qt::PointingHandCursor);
    pbCurrentColor_->setToolTip(QStringLiteral("自定义颜色"));
    connect(pbCurrentColor_, &QPushButton::clicked, this, &DrawSettings::onCurrentColor);

    // 预设颜色（两排 8 色）
    const QStringList colorList = {
        "#000000", "#808080", "#800000", "#F7883A", "#308430", "#385AD3", "#800080", "#009999",
        "#FFFFFF", "#C0C0C0", "#FB3838", "#FFFF00", "#99CC00", "#3894E4", "#F31BF3", "#16DCDC"
    };
    for (const QString &color : colorList) {
        auto *btn = new ColorSwatchButton(QColor(color), this);
        connect(btn, &QPushButton::clicked, this, &DrawSettings::onColor);
        colorBtns_.push_back(btn);
    }

    penWidthLabel_ = makeLabel(QStringLiteral("线宽"));
    colorLabel_ = makeLabel(QStringLiteral("颜色"));

    // 线宽分段控件背景容器
    auto *penWidthContainer = new QWidget(this);
    penWidthContainer->setObjectName(QStringLiteral("DrawSettingsSegment"));
    auto *segLayout = new QHBoxLayout(penWidthContainer);
    segLayout->setContentsMargins(Util::scaleSize(3), Util::scaleSize(3),
                                  Util::scaleSize(3), Util::scaleSize(3));
    segLayout->setSpacing(Util::scaleSize(2));
    for (auto *btn : penWidthBtns_) {
        segLayout->addWidget(btn);
    }

    // 颜色网格：2排 × 8列
    auto *colorGrid = new QWidget(this);
    auto *gridLayout = new QVBoxLayout(colorGrid);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(Util::scaleSize(1));
    auto *colorRow1 = new QHBoxLayout();
    auto *colorRow2 = new QHBoxLayout();
    colorRow1->setSpacing(Util::scaleSize(1));
    colorRow2->setSpacing(Util::scaleSize(1));
    for (int i = 0; i < 8; ++i) {
        colorRow1->addWidget(colorBtns_[i]);
        colorRow2->addWidget(colorBtns_[i + 8]);
    }
    colorRow1->addStretch();
    colorRow2->addStretch();
    gridLayout->addLayout(colorRow1);
    gridLayout->addLayout(colorRow2);

    // Keep the original compact one-row panel. The controls are intentionally
    // grouped with short labels and separators so the panel stays within the
    // toolbar's 44px height.
    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("DrawSettingsDivider"));
    divider->setFixedWidth(Util::scaleSize(1));

    row1_ = new QHBoxLayout();
    row1_->addWidget(penWidthLabel_);
    row1_->addWidget(penWidthContainer);
    row1_->addSpacing(Util::scaleSize(3));
    row1_->addWidget(divider, 0, Qt::AlignVCenter);
    row1_->addSpacing(Util::scaleSize(3));
    row1_->addWidget(colorLabel_);
    row1_->addWidget(colorGrid);
    row1_->addSpacing(Util::scaleSize(3));
    row1_->addWidget(sizeList_);
    row1_->addSpacing(Util::scaleSize(3));
    row1_->addWidget(pbCurrentColor_);
    row1_->addStretch();

    QVBoxLayout *mLayout = new QVBoxLayout(this);
    mLayout->addLayout(row1_);

    rescaleForDpi();
    syncPresetSelection();
}

void DrawSettings::rescaleForDpi()
{
    const int penW = Util::scaleSize(26);
    const int penH = Util::scaleSize(20);
    for (QPushButton *btn : penWidthBtns_) {
        btn->setFixedSize(penW, penH);
    }

    // Slightly larger than the old dots, while still fitting two rows in 44px.
    const int colorSize = Util::scaleSize(15);
    for (QPushButton *btn : colorBtns_) {
        btn->setFixedSize(colorSize, colorSize);
    }

    const int labelW = Util::scaleSize(28);
    if (penWidthLabel_) penWidthLabel_->setFixedWidth(labelW);
    if (colorLabel_) colorLabel_->setFixedWidth(labelW);

    pbCurrentColor_->setFixedSize(Util::scaleSize(22), Util::scaleSize(22));
    refreshCurrentColorButton();

    sizeList_->setFixedWidth(Util::scaleSize(56));

    if (auto *divider = findChild<QFrame*>(QStringLiteral("DrawSettingsDivider"))) {
        divider->setFixedWidth(Util::scaleSize(1));
        divider->setFixedHeight(Util::scaleSize(18));
    }

    if (row1_) {
        row1_->setSpacing(Util::scaleSize(4));
    }
    if (QVBoxLayout *main = qobject_cast<QVBoxLayout*>(layout())) {
        Util::scaleLayoutMargins(main, 6, 3, 6, 3);
        main->setSpacing(0);
    }

    setFixedSize(Util::scaleSize(400), Util::scaleSize(44));
}

void DrawSettings::refreshCurrentColorButton()
{
    if (!pbCurrentColor_) {
        return;
    }
    const int radius = qMax(1, Util::scaleSize(6));
    pbCurrentColor_->setStyleSheet(QString(
        "QPushButton{background-color:%1; border:1px solid rgba(128,128,128,120); border-radius:%2px;}")
        .arg(s_currentColor.name()).arg(radius));
}

void DrawSettings::syncPresetSelection()
{
    const QString current = s_currentColor.name(QColor::HexRgb);
    for (QPushButton *btn : colorBtns_) {
        const bool match = btn->property("color").toString().compare(current, Qt::CaseInsensitive) == 0;
        btn->setChecked(match);
    }
}

int DrawSettings::penWidth()
{
    return s_penWidth;
}

int DrawSettings::fontSize()
{
    return s_fontSize;
}

QColor DrawSettings::currentColor()
{
    return s_currentColor;
}

void DrawSettings::onFontSizeChanged(const QString &text)
{
    bool ok = false;
    int size = text.toInt(&ok);
    if (ok) {
        s_fontSize = size;
        emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
    }
}

void DrawSettings::onCurrentColor()
{
    QColorDialog dlg(s_currentColor, this);
    // 使用通用DPI适配方法调整对话框按钮大小
    int dlgBtnWidth = Util::scaleSize(80);
    int dlgBtnHeight = Util::scaleSize(25);
    dlg.setStyleSheet(QString("QPushButton{ width: %1px; height: %2px;};").arg(dlgBtnWidth).arg(dlgBtnHeight));
    if (QDialog::Accepted == dlg.exec()) {
        s_currentColor = dlg.selectedColor();
        refreshCurrentColorButton();
        syncPresetSelection();
        emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
    }
}

void DrawSettings::onColor()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        s_currentColor = QColor(btn->property("color").toString());
        refreshCurrentColorButton();
        syncPresetSelection();
        emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
    }
}

void DrawSettings::onPenWidthSelected()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }

    btn->setChecked(true);
    s_penWidth = btn->property("value").toInt();
    for (auto *b : penWidthBtns_) {
        if (b != btn) {
            b->setChecked(false);
        }
    }
    emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
}
