/********************************************************************************
** Form generated from reading UI file 'Settings.ui'
**
** Created by: Qt User Interface Compiler version 5.6.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
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
            Settings->setObjectName(QStringLiteral("Settings"));
        Settings->resize(408, 279);
        Settings->setMaximumSize(QSize(450, 279));
        verticalLayout = new QVBoxLayout(Settings);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        tabWidget = new QTabWidget(Settings);
        tabWidget->setObjectName(QStringLiteral("tabWidget"));
        tab = new QWidget();
        tab->setObjectName(QStringLiteral("tab"));
        verticalLayout_2 = new QVBoxLayout(tab);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        cbAutoStart = new QCheckBox(tab);
        cbAutoStart->setObjectName(QStringLiteral("cbAutoStart"));

        horizontalLayout_2->addWidget(cbAutoStart);

        horizontalSpacer = new QSpacerItem(313, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout_2->addLayout(horizontalLayout_2);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setSpacing(20);
        horizontalLayout_7->setObjectName(QStringLiteral("horizontalLayout_7"));
        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        label = new QLabel(tab);
        label->setObjectName(QStringLiteral("label"));

        horizontalLayout_4->addWidget(label);

        labelTips = new QLabel(tab);
        labelTips->setObjectName(QStringLiteral("labelTips"));

        horizontalLayout_4->addWidget(labelTips);

        leScreenshot = new QLineEdit(tab);
        leScreenshot->setObjectName(QStringLiteral("leScreenshot"));
        leScreenshot->setMaximumSize(QSize(16777215, 16777215));
        leScreenshot->setReadOnly(true);

        horizontalLayout_4->addWidget(leScreenshot);

        pbScreenshotStatus = new QPushButton(tab);
        pbScreenshotStatus->setObjectName(QStringLiteral("pbScreenshotStatus"));

        horizontalLayout_4->addWidget(pbScreenshotStatus);


        horizontalLayout_7->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setSpacing(6);
        horizontalLayout_5->setObjectName(QStringLiteral("horizontalLayout_5"));
        label_2 = new QLabel(tab);
        label_2->setObjectName(QStringLiteral("label_2"));

        horizontalLayout_5->addWidget(label_2);

        lePin = new QLineEdit(tab);
        lePin->setObjectName(QStringLiteral("lePin"));
        lePin->setMaximumSize(QSize(16777215, 16777215));
        lePin->setReadOnly(true);

        horizontalLayout_5->addWidget(lePin);

        pbPinStatus = new QPushButton(tab);
        pbPinStatus->setObjectName(QStringLiteral("pbPinStatus"));

        horizontalLayout_5->addWidget(pbPinStatus);


        horizontalLayout_7->addLayout(horizontalLayout_5);


        verticalLayout_2->addLayout(horizontalLayout_7);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setSpacing(17);
        horizontalLayout_6->setObjectName(QStringLiteral("horizontalLayout_6"));
        label_6 = new QLabel(tab);
        label_6->setObjectName(QStringLiteral("label_6"));

        horizontalLayout_6->addWidget(label_6);

        leUploadImageUrl = new QLineEdit(tab);
        leUploadImageUrl->setObjectName(QStringLiteral("leUploadImageUrl"));

        horizontalLayout_6->addWidget(leUploadImageUrl);


        verticalLayout_2->addLayout(horizontalLayout_6);

        cbPinNoBorder = new QCheckBox(tab);
        cbPinNoBorder->setObjectName(QStringLiteral("cbPinNoBorder"));

        verticalLayout_2->addWidget(cbPinNoBorder);

        checkBox = new QCheckBox(tab);
        checkBox->setObjectName(QStringLiteral("checkBox"));

        verticalLayout_2->addWidget(checkBox);

        widget = new QWidget(tab);
        widget->setObjectName(QStringLiteral("widget"));
        horizontalLayout_8 = new QHBoxLayout(widget);
        horizontalLayout_8->setSpacing(6);
        horizontalLayout_8->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_8->setObjectName(QStringLiteral("horizontalLayout_8"));
        horizontalLayout_8->setContentsMargins(0, 0, 20, 0);
        label_8 = new QLabel(widget);
        label_8->setObjectName(QStringLiteral("label_8"));

        horizontalLayout_8->addWidget(label_8);

        rbRGB = new QRadioButton(widget);
        rbRGB->setObjectName(QStringLiteral("rbRGB"));

        horizontalLayout_8->addWidget(rbRGB);

        rbHexadecimal = new QRadioButton(widget);
        rbHexadecimal->setObjectName(QStringLiteral("rbHexadecimal"));

        horizontalLayout_8->addWidget(rbHexadecimal);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_8->addItem(horizontalSpacer_4);


        verticalLayout_2->addWidget(widget);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        verticalLayout_2->addItem(horizontalSpacer_3);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        tabWidget->addTab(tab, QString());
        tab_3 = new QWidget();
        tab_3->setObjectName(QStringLiteral("tab_3"));
        gridLayout_2 = new QGridLayout(tab_3);
        gridLayout_2->setSpacing(2);
        gridLayout_2->setContentsMargins(11, 11, 11, 11);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        gridLayout_2->setContentsMargins(2, 2, 2, 2);
        tablePath = new QTableWidget(tab_3);
        tablePath->setObjectName(QStringLiteral("tablePath"));
        tablePath->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        tablePath->horizontalHeader()->setDefaultSectionSize(50);

        gridLayout_2->addWidget(tablePath, 0, 0, 1, 1);

        label_7 = new QLabel(tab_3);
        label_7->setObjectName(QStringLiteral("label_7"));

        gridLayout_2->addWidget(label_7, 1, 0, 1, 1);

        tabWidget->addTab(tab_3, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QStringLiteral("tab_2"));
        horizontalLayout_3 = new QHBoxLayout(tab_2);
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        label_3 = new QLabel(tab_2);
        label_3->setObjectName(QStringLiteral("label_3"));

        verticalLayout_3->addWidget(label_3);

        label_4 = new QLabel(tab_2);
        label_4->setObjectName(QStringLiteral("label_4"));

        verticalLayout_3->addWidget(label_4);

        label_5 = new QLabel(tab_2);
        label_5->setObjectName(QStringLiteral("label_5"));

        verticalLayout_3->addWidget(label_5);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer_2);


        horizontalLayout_3->addLayout(verticalLayout_3);

        tabWidget->addTab(tab_2, QString());

        verticalLayout->addWidget(tabWidget);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        pbRevert = new QPushButton(Settings);
        pbRevert->setObjectName(QStringLiteral("pbRevert"));
        pbRevert->setMinimumSize(QSize(75, 25));
        pbRevert->setMaximumSize(QSize(75, 25));

        horizontalLayout->addWidget(pbRevert);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(Settings);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(Settings);
    } // setupUi

    void retranslateUi(QWidget *Settings)
    {
        Settings->setWindowTitle(QApplication::translate("Settings", "Settings", Q_NULLPTR));
        cbAutoStart->setText(QApplication::translate("Settings", "\345\274\200\346\234\272\350\207\252\345\220\257\345\212\250", Q_NULLPTR));
        label->setText(QApplication::translate("Settings", "\346\210\252\345\233\276\345\277\253\346\215\267\351\224\256\357\274\232", Q_NULLPTR));
        labelTips->setText(QApplication::translate("Settings", "tips", Q_NULLPTR));
        leScreenshot->setPlaceholderText(QApplication::translate("Settings", "\347\202\271\345\207\273\351\234\200\350\246\201\350\256\276\347\275\256\347\232\204\345\277\253\346\215\267\351\224\256", Q_NULLPTR));
        pbScreenshotStatus->setText(QString());
        label_2->setText(QApplication::translate("Settings", "\350\264\264\345\233\276\345\277\253\346\215\267\351\224\256\357\274\232", Q_NULLPTR));
        lePin->setPlaceholderText(QApplication::translate("Settings", "\347\202\271\345\207\273\351\234\200\350\246\201\350\256\276\347\275\256\347\232\204\345\277\253\346\215\267\351\224\256", Q_NULLPTR));
        pbPinStatus->setText(QString());
        label_6->setText(QApplication::translate("Settings", "\345\233\276\345\272\212\345\234\260\345\235\200\357\274\232", Q_NULLPTR));
        cbPinNoBorder->setText(QApplication::translate("Settings", "\350\264\264\345\233\276\346\227\240\350\276\271\346\241\206", Q_NULLPTR));
        checkBox->setText(QApplication::translate("Settings", "\351\223\276\346\216\245\345\244\215\345\210\266\346\210\220Markdown\346\240\274\345\274\217", Q_NULLPTR));
        label_8->setText(QApplication::translate("Settings", "\351\242\234\350\211\262\346\240\274\345\274\217\346\230\276\347\244\272(C\351\224\256\345\244\215\345\210\266\351\242\234\350\211\262)\357\274\232", Q_NULLPTR));
        rbRGB->setText(QApplication::translate("Settings", "RGB", Q_NULLPTR));
        rbHexadecimal->setText(QApplication::translate("Settings", "\345\215\201\345\205\255\350\277\233\345\210\266", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tab), QApplication::translate("Settings", "\345\270\270\350\247\204\350\256\276\347\275\256", Q_NULLPTR));
        label_7->setText(QApplication::translate("Settings", "\346\217\220\347\244\272\357\274\232\351\274\240\346\240\207\345\267\246\351\224\256\345\217\214\345\207\273\345\217\257\345\244\215\345\210\266\350\267\257\345\276\204\345\210\260\345\211\252\345\210\207\346\235\277\343\200\202", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tab_3), QApplication::translate("Settings", "\350\267\257\345\276\204", Q_NULLPTR));
        label_3->setText(QApplication::translate("Settings", "ntscreenshot", Q_NULLPTR));
        label_4->setText(QApplication::translate("Settings", "\347\211\210\346\234\254 v1.0.0", Q_NULLPTR));
        label_5->setText(QApplication::translate("Settings", "\345\261\217\345\271\225\346\210\252\345\233\276\345\267\245\345\205\267\357\274\214\345\217\257\344\273\245\350\264\264\345\233\276\343\200\201\347\273\230\345\233\276\343\200\201\344\270\212\344\274\240\345\233\276\345\272\212", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QApplication::translate("Settings", "\345\205\263\344\272\216", Q_NULLPTR));
        pbRevert->setText(QApplication::translate("Settings", "\346\201\242\345\244\215\351\273\230\350\256\244", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class Settings: public Ui_Settings {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGS_H
