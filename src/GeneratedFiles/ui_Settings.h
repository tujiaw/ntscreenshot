/********************************************************************************
** Form generated from reading UI file 'Settings.ui'
**
** Created by: Qt User Interface Compiler version 5.12.9
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Settings
{
public:
    QVBoxLayout *verticalLayout;
    QTabWidget *tabWidget;
    QWidget *tab;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout_2;
    QCheckBox *cbAutoStart;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_7;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label;
    QLabel *labelTips;
    QLineEdit *leScreenshot;
    QPushButton *pbScreenshotStatus;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_2;
    QLineEdit *lePin;
    QPushButton *pbPinStatus;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_6;
    QLineEdit *leUploadImageUrl;
    QCheckBox *cbPinNoBorder;
    QCheckBox *checkBox;
    QWidget *widget;
    QHBoxLayout *horizontalLayout_8;
    QLabel *label_8;
    QRadioButton *rbRGB;
    QRadioButton *rbHexadecimal;
    QSpacerItem *horizontalSpacer_4;
    QSpacerItem *horizontalSpacer_3;
    QSpacerItem *verticalSpacer;
    QWidget *tab_4;
    QVBoxLayout *verticalLayout_5;
    QWidget *imagePathWidget;
    QVBoxLayout *verticalLayout_4;
    QCheckBox *cbAutoSave;
    QHBoxLayout *horizontalLayout_9;
    QLabel *label_9;
    QLineEdit *leSavePath;
    QHBoxLayout *horizontalLayout_10;
    QSpacerItem *horizontalSpacer_5;
    QPushButton *pbOpenImagePath;
    QPushButton *pbModifyImagePath;
    QSpacerItem *verticalSpacer_3;
    QWidget *tab_3;
    QGridLayout *gridLayout_2;
    QTableWidget *tablePath;
    QLabel *label_7;
    QWidget *tab_2;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout_3;
    QLabel *label_3;
    QLabel *label_4;
    QLabel *label_5;
    QSpacerItem *verticalSpacer_2;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pbRevert;

    void setupUi(QWidget *Settings)
    {
        if (Settings->objectName().isEmpty())
            Settings->setObjectName(QString::fromUtf8("Settings"));
        Settings->resize(408, 279);
        Settings->setMaximumSize(QSize(450, 279));
        verticalLayout = new QVBoxLayout(Settings);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(6, 6, 6, 6);
        tabWidget = new QTabWidget(Settings);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tab = new QWidget();
        tab->setObjectName(QString::fromUtf8("tab"));
        verticalLayout_2 = new QVBoxLayout(tab);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        cbAutoStart = new QCheckBox(tab);
        cbAutoStart->setObjectName(QString::fromUtf8("cbAutoStart"));

        horizontalLayout_2->addWidget(cbAutoStart);

        horizontalSpacer = new QSpacerItem(313, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout_2->addLayout(horizontalLayout_2);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setSpacing(20);
        horizontalLayout_7->setObjectName(QString::fromUtf8("horizontalLayout_7"));
        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        label = new QLabel(tab);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout_4->addWidget(label);

        labelTips = new QLabel(tab);
        labelTips->setObjectName(QString::fromUtf8("labelTips"));

        horizontalLayout_4->addWidget(labelTips);

        leScreenshot = new QLineEdit(tab);
        leScreenshot->setObjectName(QString::fromUtf8("leScreenshot"));
        leScreenshot->setMaximumSize(QSize(16777215, 16777215));
        leScreenshot->setReadOnly(true);

        horizontalLayout_4->addWidget(leScreenshot);

        pbScreenshotStatus = new QPushButton(tab);
        pbScreenshotStatus->setObjectName(QString::fromUtf8("pbScreenshotStatus"));

        horizontalLayout_4->addWidget(pbScreenshotStatus);


        horizontalLayout_7->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setSpacing(6);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        label_2 = new QLabel(tab);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        horizontalLayout_5->addWidget(label_2);

        lePin = new QLineEdit(tab);
        lePin->setObjectName(QString::fromUtf8("lePin"));
        lePin->setMaximumSize(QSize(16777215, 16777215));
        lePin->setReadOnly(true);

        horizontalLayout_5->addWidget(lePin);

        pbPinStatus = new QPushButton(tab);
        pbPinStatus->setObjectName(QString::fromUtf8("pbPinStatus"));

        horizontalLayout_5->addWidget(pbPinStatus);


        horizontalLayout_7->addLayout(horizontalLayout_5);


        verticalLayout_2->addLayout(horizontalLayout_7);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setSpacing(17);
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        label_6 = new QLabel(tab);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        horizontalLayout_6->addWidget(label_6);

        leUploadImageUrl = new QLineEdit(tab);
        leUploadImageUrl->setObjectName(QString::fromUtf8("leUploadImageUrl"));

        horizontalLayout_6->addWidget(leUploadImageUrl);


        verticalLayout_2->addLayout(horizontalLayout_6);

        cbPinNoBorder = new QCheckBox(tab);
        cbPinNoBorder->setObjectName(QString::fromUtf8("cbPinNoBorder"));

        verticalLayout_2->addWidget(cbPinNoBorder);

        checkBox = new QCheckBox(tab);
        checkBox->setObjectName(QString::fromUtf8("checkBox"));

        verticalLayout_2->addWidget(checkBox);

        widget = new QWidget(tab);
        widget->setObjectName(QString::fromUtf8("widget"));
        horizontalLayout_8 = new QHBoxLayout(widget);
        horizontalLayout_8->setSpacing(6);
        horizontalLayout_8->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_8->setObjectName(QString::fromUtf8("horizontalLayout_8"));
        horizontalLayout_8->setContentsMargins(0, 0, 20, 0);
        label_8 = new QLabel(widget);
        label_8->setObjectName(QString::fromUtf8("label_8"));

        horizontalLayout_8->addWidget(label_8);

        rbRGB = new QRadioButton(widget);
        rbRGB->setObjectName(QString::fromUtf8("rbRGB"));

        horizontalLayout_8->addWidget(rbRGB);

        rbHexadecimal = new QRadioButton(widget);
        rbHexadecimal->setObjectName(QString::fromUtf8("rbHexadecimal"));

        horizontalLayout_8->addWidget(rbHexadecimal);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_8->addItem(horizontalSpacer_4);


        verticalLayout_2->addWidget(widget);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        verticalLayout_2->addItem(horizontalSpacer_3);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        tabWidget->addTab(tab, QString());
        tab_4 = new QWidget();
        tab_4->setObjectName(QString::fromUtf8("tab_4"));
        verticalLayout_5 = new QVBoxLayout(tab_4);
        verticalLayout_5->setSpacing(6);
        verticalLayout_5->setContentsMargins(11, 11, 11, 11);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(6, 3, 6, 6);
        imagePathWidget = new QWidget(tab_4);
        imagePathWidget->setObjectName(QString::fromUtf8("imagePathWidget"));
        verticalLayout_4 = new QVBoxLayout(imagePathWidget);
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setContentsMargins(11, 11, 11, 11);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        cbAutoSave = new QCheckBox(imagePathWidget);
        cbAutoSave->setObjectName(QString::fromUtf8("cbAutoSave"));

        verticalLayout_4->addWidget(cbAutoSave);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setSpacing(6);
        horizontalLayout_9->setObjectName(QString::fromUtf8("horizontalLayout_9"));
        label_9 = new QLabel(imagePathWidget);
        label_9->setObjectName(QString::fromUtf8("label_9"));

        horizontalLayout_9->addWidget(label_9);

        leSavePath = new QLineEdit(imagePathWidget);
        leSavePath->setObjectName(QString::fromUtf8("leSavePath"));
        leSavePath->setReadOnly(true);

        horizontalLayout_9->addWidget(leSavePath);


        verticalLayout_4->addLayout(horizontalLayout_9);

        horizontalLayout_10 = new QHBoxLayout();
        horizontalLayout_10->setSpacing(6);
        horizontalLayout_10->setObjectName(QString::fromUtf8("horizontalLayout_10"));
        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_10->addItem(horizontalSpacer_5);

        pbOpenImagePath = new QPushButton(imagePathWidget);
        pbOpenImagePath->setObjectName(QString::fromUtf8("pbOpenImagePath"));
        pbOpenImagePath->setMinimumSize(QSize(75, 25));
        pbOpenImagePath->setMaximumSize(QSize(75, 25));

        horizontalLayout_10->addWidget(pbOpenImagePath);

        pbModifyImagePath = new QPushButton(imagePathWidget);
        pbModifyImagePath->setObjectName(QString::fromUtf8("pbModifyImagePath"));
        pbModifyImagePath->setMinimumSize(QSize(75, 25));
        pbModifyImagePath->setMaximumSize(QSize(75, 25));

        horizontalLayout_10->addWidget(pbModifyImagePath);


        verticalLayout_4->addLayout(horizontalLayout_10);


        verticalLayout_5->addWidget(imagePathWidget);

        verticalSpacer_3 = new QSpacerItem(20, 112, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_5->addItem(verticalSpacer_3);

        tabWidget->addTab(tab_4, QString());
        tab_3 = new QWidget();
        tab_3->setObjectName(QString::fromUtf8("tab_3"));
        gridLayout_2 = new QGridLayout(tab_3);
        gridLayout_2->setSpacing(2);
        gridLayout_2->setContentsMargins(11, 11, 11, 11);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        gridLayout_2->setContentsMargins(2, 2, 2, 2);
        tablePath = new QTableWidget(tab_3);
        tablePath->setObjectName(QString::fromUtf8("tablePath"));
        tablePath->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        tablePath->horizontalHeader()->setDefaultSectionSize(50);

        gridLayout_2->addWidget(tablePath, 0, 0, 1, 1);

        label_7 = new QLabel(tab_3);
        label_7->setObjectName(QString::fromUtf8("label_7"));

        gridLayout_2->addWidget(label_7, 1, 0, 1, 1);

        tabWidget->addTab(tab_3, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QString::fromUtf8("tab_2"));
        horizontalLayout_3 = new QHBoxLayout(tab_2);
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        label_3 = new QLabel(tab_2);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        verticalLayout_3->addWidget(label_3);

        label_4 = new QLabel(tab_2);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        verticalLayout_3->addWidget(label_4);

        label_5 = new QLabel(tab_2);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        verticalLayout_3->addWidget(label_5);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer_2);


        horizontalLayout_3->addLayout(verticalLayout_3);

        tabWidget->addTab(tab_2, QString());

        verticalLayout->addWidget(tabWidget);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        pbRevert = new QPushButton(Settings);
        pbRevert->setObjectName(QString::fromUtf8("pbRevert"));
        pbRevert->setMinimumSize(QSize(75, 25));
        pbRevert->setMaximumSize(QSize(75, 25));

        horizontalLayout->addWidget(pbRevert);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(Settings);

        tabWidget->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(Settings);
    } // setupUi

    void retranslateUi(QWidget *Settings)
    {
        Settings->setWindowTitle(QApplication::translate("Settings", "Settings", nullptr));
        cbAutoStart->setText(QApplication::translate("Settings", "\345\274\200\346\234\272\350\207\252\345\220\257\345\212\250", nullptr));
        label->setText(QApplication::translate("Settings", "\346\210\252\345\233\276\345\277\253\346\215\267\351\224\256\357\274\232", nullptr));
        labelTips->setText(QApplication::translate("Settings", "tips", nullptr));
        leScreenshot->setPlaceholderText(QApplication::translate("Settings", "\347\202\271\345\207\273\351\234\200\350\246\201\350\256\276\347\275\256\347\232\204\345\277\253\346\215\267\351\224\256", nullptr));
        pbScreenshotStatus->setText(QString());
        label_2->setText(QApplication::translate("Settings", "\350\264\264\345\233\276\345\277\253\346\215\267\351\224\256\357\274\232", nullptr));
        lePin->setPlaceholderText(QApplication::translate("Settings", "\347\202\271\345\207\273\351\234\200\350\246\201\350\256\276\347\275\256\347\232\204\345\277\253\346\215\267\351\224\256", nullptr));
        pbPinStatus->setText(QString());
        label_6->setText(QApplication::translate("Settings", "\345\233\276\345\272\212\345\234\260\345\235\200\357\274\232", nullptr));
        cbPinNoBorder->setText(QApplication::translate("Settings", "\350\264\264\345\233\276\346\227\240\350\276\271\346\241\206", nullptr));
        checkBox->setText(QApplication::translate("Settings", "\351\223\276\346\216\245\345\244\215\345\210\266\346\210\220Markdown\346\240\274\345\274\217", nullptr));
        label_8->setText(QApplication::translate("Settings", "\351\242\234\350\211\262\346\240\274\345\274\217\346\230\276\347\244\272(C\351\224\256\345\244\215\345\210\266\351\242\234\350\211\262)\357\274\232", nullptr));
        rbRGB->setText(QApplication::translate("Settings", "RGB", nullptr));
        rbHexadecimal->setText(QApplication::translate("Settings", "\345\215\201\345\205\255\350\277\233\345\210\266", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab), QApplication::translate("Settings", "\345\270\270\350\247\204\350\256\276\347\275\256", nullptr));
        cbAutoSave->setText(QApplication::translate("Settings", "\350\207\252\345\212\250\344\277\235\345\255\230", nullptr));
        label_9->setText(QApplication::translate("Settings", "\350\267\257\345\276\204\357\274\232", nullptr));
        pbOpenImagePath->setText(QApplication::translate("Settings", "\346\211\223\345\274\200", nullptr));
        pbModifyImagePath->setText(QApplication::translate("Settings", "\346\233\264\346\224\271", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_4), QApplication::translate("Settings", "\345\233\276\347\211\207", nullptr));
        label_7->setText(QApplication::translate("Settings", "\346\217\220\347\244\272\357\274\232\351\274\240\346\240\207\345\267\246\351\224\256\345\217\214\345\207\273\345\217\257\345\244\215\345\210\266\350\267\257\345\276\204\345\210\260\345\211\252\345\210\207\346\235\277\343\200\202", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_3), QApplication::translate("Settings", "\350\267\257\345\276\204", nullptr));
        label_3->setText(QApplication::translate("Settings", "ntscreenshot", nullptr));
        label_4->setText(QApplication::translate("Settings", "\347\211\210\346\234\254 v1.0.0", nullptr));
        label_5->setText(QApplication::translate("Settings", "\345\261\217\345\271\225\346\210\252\345\233\276\345\267\245\345\205\267\357\274\214\345\217\257\344\273\245\350\264\264\345\233\276\343\200\201\347\273\230\345\233\276\343\200\201\344\270\212\344\274\240\345\233\276\345\272\212", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QApplication::translate("Settings", "\345\205\263\344\272\216", nullptr));
        pbRevert->setText(QApplication::translate("Settings", "\346\201\242\345\244\215\351\273\230\350\256\244", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Settings: public Ui_Settings {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGS_H
