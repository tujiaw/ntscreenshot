#include "Settings.h"
#include <QDebug>
#include <QKeyEvent>
#include <QClipboard>
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
    initTablePath();

    connect(ui.cbAutoStart, &QCheckBox::clicked, this, &Settings::onAutoStartClicked);
    connect(ui.cbPinNoBorder, &QCheckBox::clicked, this, &Settings::onPinNoBorder);
    connect(ui.pbRevert, &QPushButton::clicked, this, &Settings::onRevertClicked);
    connect(ui.leUploadImageUrl, &QLineEdit::textChanged, this, &Settings::onUploadImageUrlChanged);

    readData();
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
    data.push_back(QStringList() << QStringLiteral("ÅäÖÃ") << Util::getConfigDir());
    data.push_back(QStringList() << QStringLiteral("ÈÕÖ¾") << Util::getLogsDir());

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
    ui.tablePath->setColumnCount(data[0].size());

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
