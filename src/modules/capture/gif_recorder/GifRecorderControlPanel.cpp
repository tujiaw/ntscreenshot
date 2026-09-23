#include "GifRecorderControlPanel.h"

#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"
#include "core/platform/Util.h"

#include <QPainter>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

GifRecorderControlPanel::GifRecorderControlPanel(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName(QStringLiteral("GifRecorderControlPanel"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto *fpsLabel = new QLabel(QStringLiteral("FPS"), this);
    fpsLabel->setObjectName(QStringLiteral("gifFpsLabel"));
    fpsEdit_ = new QLineEdit(QStringLiteral("10"), this);
    fpsEdit_->setObjectName(QStringLiteral("gifFpsEdit"));
    fpsEdit_->setValidator(new QIntValidator(1, 30, fpsEdit_));
    fpsEdit_->setAlignment(Qt::AlignCenter);
    fpsEdit_->setMaxLength(2);
    fpsEdit_->setFixedSize(34, 22);
    fpsEdit_->setToolTip(QStringLiteral("输入 FPS（1–30）"));

    sizeLabel_ = new QLabel(this);
    sizeLabel_->setObjectName(QStringLiteral("gifSizeLabel"));
    elapsedLabel_ = new QLabel(QStringLiteral("00:00 · 0"), this);
    elapsedLabel_->setObjectName(QStringLiteral("gifElapsedLabel"));
    recordButton_ = new QPushButton(QStringLiteral("录制"), this);
    copyButton_ = new QPushButton(QStringLiteral("完成/复制"), this);
    openButton_ = new QPushButton(QStringLiteral("完成/打开"), this);
    cancelButton_ = new QToolButton(this);

    recordButton_->setObjectName(QStringLiteral("gifRecordButton"));
    copyButton_->setObjectName(QStringLiteral("gifCopyButton"));
    copyButton_->setToolTip(QStringLiteral("复制 GIF 文件到剪贴板"));
    openButton_->setObjectName(QStringLiteral("gifOpenButton"));
    openButton_->setToolTip(QStringLiteral("打开已保存的 GIF 文件"));
    cancelButton_->setObjectName(QStringLiteral("gifCloseButton"));
    recordButton_->setFixedSize(56, 24);
    copyButton_->setMinimumWidth(copyButton_->fontMetrics().horizontalAdvance(copyButton_->text()) + 28);
    openButton_->setMinimumWidth(openButton_->fontMetrics().horizontalAdvance(openButton_->text()) + 28);
    copyButton_->setFixedHeight(24);
    openButton_->setFixedHeight(24);
    cancelButton_->setFixedSize(22, 22);
    cancelButton_->setIconSize(QSize(12, 12));
    cancelButton_->setIcon(ThemeIcon::icon(QStringLiteral("icon_window_close.png")));
    cancelButton_->setToolTip(QStringLiteral("取消录制 (Esc)"));

    layout->addWidget(fpsLabel);
    layout->addWidget(fpsEdit_);
    layout->addWidget(sizeLabel_);
    layout->addWidget(elapsedLabel_);
    layout->addSpacing(8);
    layout->addWidget(recordButton_);
    layout->addWidget(copyButton_);
    layout->addWidget(openButton_);
    layout->addWidget(cancelButton_);

    connect(recordButton_, &QPushButton::clicked, this, &GifRecorderControlPanel::sigRecordClicked);
    connect(copyButton_, &QPushButton::clicked, this, &GifRecorderControlPanel::sigCopyClicked);
    connect(openButton_, &QPushButton::clicked, this, &GifRecorderControlPanel::sigOpenClicked);
    connect(cancelButton_, &QToolButton::clicked, this, &GifRecorderControlPanel::sigCancelClicked);
    setPreparing();
}

void GifRecorderControlPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = ThemeManager::tokens();
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(tokens.border, 1));
    painter.setBrush(tokens.surfaceRaised);
    painter.drawRoundedRect(r, Util::scaleSize(8), Util::scaleSize(8));
}

void GifRecorderControlPanel::applyTheme()
{
}

int GifRecorderControlPanel::framesPerSecond() const
{
    bool ok = false;
    const int fps = fpsEdit_->text().toInt(&ok);
    return ok ? qBound(1, fps, 30) : 10;
}

void GifRecorderControlPanel::setCaptureSize(const QSize &size)
{
    sizeLabel_->setText(QStringLiteral("%1×%2").arg(size.width()).arg(size.height()));
}

void GifRecorderControlPanel::setElapsedMilliseconds(qint64 milliseconds, int frames)
{
    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    elapsedLabel_->setText(QStringLiteral("%1:%2 · %3")
                               .arg(minutes, 2, 10, QLatin1Char('0'))
                               .arg(seconds, 2, 10, QLatin1Char('0'))
                               .arg(frames));
}

void GifRecorderControlPanel::setPreparing()
{
    fpsEdit_->setEnabled(true);
    fpsEdit_->show();
    recordButton_->setEnabled(true);
    recordButton_->show();
    recordButton_->setText(QStringLiteral("录制"));
    copyButton_->hide();
    openButton_->hide();
    cancelButton_->setEnabled(true);
    cancelButton_->setToolTip(QStringLiteral("取消录制 (Esc)"));
    setElapsedMilliseconds(0, 0);
    adjustSize();
}

void GifRecorderControlPanel::setRecording()
{
    fpsEdit_->setEnabled(false);
    recordButton_->hide();
    copyButton_->show();
    copyButton_->setEnabled(true);
    openButton_->show();
    openButton_->setEnabled(true);
    cancelButton_->setEnabled(true);
    setElapsedMilliseconds(0, 0);
    adjustSize();
}

void GifRecorderControlPanel::setStarting()
{
    fpsEdit_->setEnabled(false);
    recordButton_->setEnabled(false);
    copyButton_->setEnabled(false);
    openButton_->setEnabled(false);
    cancelButton_->setEnabled(true);
    elapsedLabel_->setText(QStringLiteral("准备中…"));
}

void GifRecorderControlPanel::setEncoding()
{
    fpsEdit_->setEnabled(false);
    recordButton_->setEnabled(false);
    copyButton_->setEnabled(false);
    openButton_->setEnabled(false);
    cancelButton_->setEnabled(false);
    elapsedLabel_->setText(QStringLiteral("保存中…"));
}
