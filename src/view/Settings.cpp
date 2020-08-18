#include "Settings.h"
#include <QDebug>
#include <QKeyEvent>
#include <QClipboard>
#include <QFileDialog>
#include "component/qxtglobalshortcut/qxtglobalshortcut.h"
#include "model/SettingModel.h"
#include "common/Util.h"
#include "controller/WindowManager.h"

Settings::Settings(QWidget *parent)
    : QWidget(parent)
{
	ui.setupUi(this);
	ui.pbScreenshotStatus->setFixedSize(25, 25);
	ui.pbPinStatus->setFixedSize(25, 25);
    ui.pbScreenshotStatus->setIcon(QIcon(":/images/ok.png"));
    ui.pbPinStatus->setIcon(QIcon(":/images/ok.png"));
    QPixmap tipsPixmap(QString(":/images/tips.png"));
    ui.labelTips->setPixmap(tipsPixmap);
    ui.labelTips->setToolTip(QStringLiteral("在输入框上按下要设置的快捷键"));
    initTablePath();

    connect(ui.cbAutoStart, &QCheckBox::clicked, this, &Settings::onAutoStartClicked);
    connect(ui.cbPinNoBorder, &QCheckBox::clicked, this, &Settings::onPinNoBorder);
    connect(ui.pbRevert, &QPushButton::clicked, this, &Settings::onRevertClicked);
    connect(ui.leUploadImageUrl, &QLineEdit::textChanged, this, &Settings::onUploadImageUrlChanged);
    connect(ui.rbRGB, &QPushButton::clicked, this, &Settings::onColorShowChanged);
    connect(ui.rbHexadecimal, &QPushButton::clicked, this, &Settings::onColorShowChanged);
    connect(ui.cbAutoSave, &QCheckBox::clicked, this, &Settings::onAutoSaveChanged);
    connect(ui.pbOpenImagePath, &QPushButton::clicked, this, &Settings::onOpenImagePath);
    connect(ui.pbModifyImagePath, &QPushButton::clicked, this, &Settings::onModifyImagePath);

    readData();
    setFixedSize(400, 280);
}

Settings::~Settings()
{
}

void Settings::readData()
{
    SettingModel *setting = WindowManager::instance()->setting();
    ui.cbAutoStart->setCheckState(setting->autoStart() ? Qt::Checked : Qt::Unchecked);
    ui.cbPinNoBorder->setCheckState(setting->pinNoBorder() ? Qt::Checked : Qt::Unchecked);
    ui.leScreenshot->setText(setting->screenhotGlobalKey());
    ui.lePin->setText(setting->pinGlobalKey());
    ui.leUploadImageUrl->setText(setting->uploadImageUrl());
    ui.rbRGB->setChecked(setting->rgbColor());
    ui.rbHexadecimal->setChecked(!setting->rgbColor());

    bool autoSave = false;
    QString autoSavePath;
    setting->getAutoSaveImage(autoSave, autoSavePath);
    ui.cbAutoSave->setChecked(autoSave);
    ui.leSavePath->setText(autoSavePath);
    ui.pbOpenImagePath->setEnabled(autoSave);
    ui.pbModifyImagePath->setEnabled(autoSave);
}

void Settings::writeData()
{

}

void Settings::keyPressEvent(QKeyEvent* keyEvent)
{
	if (ui.leScreenshot->hasFocus()) {
        updateScreenshotGlobalKey(Util::strKeyEvent(keyEvent));
        emit WindowManager::instance()->sigSettingChanged();
	}
	else if (ui.lePin->hasFocus()) {
        updatePinKey(Util::strKeyEvent(keyEvent));
        emit WindowManager::instance()->sigSettingChanged();
	}
	QWidget::keyPressEvent(keyEvent);
}

void Settings::initTablePath()
{
    QList<QStringList> data;
    data.push_back(QStringList() << QStringLiteral("程序") << Util::getRunDir());
    data.push_back(QStringList() << QStringLiteral("配置") << Util::getConfigDir());
    data.push_back(QStringList() << QStringLiteral("日志") << Util::getLogsDir());

    ui.tablePath->setFrameShape(QFrame::NoFrame);
    ui.tablePath->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tablePath->setAlternatingRowColors(true);
    ui.tablePath->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.tablePath->verticalHeader()->setVisible(false);
    ui.tablePath->horizontalHeader()->setVisible(false);
    ui.tablePath->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tablePath->setFocusPolicy(Qt::NoFocus);
    ui.tablePath->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui.tablePath->setShowGrid(false);
    ui.tablePath->setMouseTracking(true);
    ui.tablePath->horizontalHeader()->setStretchLastSection(true);
    ui.tablePath->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.tablePath->setRowCount(data.size());
    ui.tablePath->setColumnCount(data.first().size());
    ui.tablePath->setTextElideMode(Qt::ElideMiddle);

    connect(ui.tablePath, &QTableWidget::cellDoubleClicked, this, &Settings::onTablePathDoubleClicked);
    for (int row = 0; row < data.size(); row++) {
        for (int col = 0; col < data[row].size(); col++) {
            QTableWidgetItem *item = new QTableWidgetItem(data[row][col]);
            ui.tablePath->setItem(row, col, item);
        }
    }
}

void Settings::updateScreenshotGlobalKey(const QString &key)
{
    if (!key.isEmpty() && WindowManager::instance()->setScreenshotGlobalKey(key)) {
        WindowManager::instance()->setting()->setScreenshotGlobalKey(key);
        ui.leScreenshot->setText(key);
        ui.pbScreenshotStatus->setIcon(QIcon(":/images/ok.png"));
    } else {
        ui.pbScreenshotStatus->setIcon(QIcon(":/images/remove.png"));
    }
}

void Settings::updatePinKey(const QString &key)
{
    if (!key.isEmpty() && WindowManager::instance()->setPinGlobalKey(key)) {
        WindowManager::instance()->setting()->setPinGlobalKey(key);
        ui.lePin->setText(key);
        ui.pbPinStatus->setIcon(QIcon(":/images/ok.png"));
    } else {
        ui.pbPinStatus->setIcon(QIcon(":/images/remove.png"));
    }
}

void Settings::onAutoStartClicked(bool checked)
{
    WindowManager::instance()->setting()->setAutoStart(checked);
    emit WindowManager::instance()->sigSettingChanged();
}

void Settings::onPinNoBorder(bool checked)
{
    WindowManager::instance()->setting()->setPinNoBorder(checked);
    emit WindowManager::instance()->sigSettingChanged();
}

void Settings::onRevertClicked()
{
    WindowManager::instance()->setting()->revertDefault();
    readData();
    emit WindowManager::instance()->sigSettingChanged();
}

void Settings::onTablePathDoubleClicked(int row, int col)
{
    QString text = ui.tablePath->item(row, col)->text();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
}

void Settings::onUploadImageUrlChanged(const QString& text)
{
    WindowManager::instance()->setting()->setUploadImageUrl(text);
}

void Settings::onColorShowChanged()
{
    if (ui.rbRGB->isChecked()) {
        WindowManager::instance()->setting()->setRgbColor(true);
    } else {
        WindowManager::instance()->setting()->setRgbColor(false);
    }
}

void Settings::onAutoSaveChanged()
{
    bool autoSave = ui.cbAutoSave->isChecked();
    ui.pbOpenImagePath->setEnabled(autoSave);
    ui.pbModifyImagePath->setEnabled(autoSave);
    WindowManager::instance()->setting()->setAutoSaveImage(autoSave, ui.leSavePath->text().trimmed());
}

void Settings::onOpenImagePath()
{
	Util::shellExecute(ui.leSavePath->text().trimmed());
}

void Settings::onModifyImagePath()
{
	QString newDir = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择截图保存目录"));
	if (!newDir.isEmpty()) {
		ui.leSavePath->setText(newDir);
		onAutoSaveChanged();
	}
}
