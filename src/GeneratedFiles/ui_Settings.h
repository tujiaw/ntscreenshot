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
    QCheckBox *cbPinNoBorder;
    QSpacerItem *horizontalSpacer;
    QGridLayout *gridLayout;
    QPushButton *pbPinStatus;
    QLabel *label_2;
    QLineEdit *lePin;
    QLabel *label;
    QLineEdit *leScreenshot;
    QPushButton *pbScreenshotStatus;
    QSpacerItem *horizontalSpacer_4;
    QSpacerItem *horizontalSpacer_5;
    QSpacerItem *horizontalSpacer_3;
    QSpacerItem *verticalSpacer;
    QWidget *tab_3;
    QGridLayout *gridLayout_2;
    QTableWidget *tablePath;
    QWidget *tab_2;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pbRevert;

    void setupUi(QWidget *Settings)
    {
        if (Settings->objectName().isEmpty())
            Settings->setObjectName(QStringLiteral("Settings"));
        Settings->resize(450, 360);
        Settings->setMaximumSize(QSize(450, 16777215));
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

        cbPinNoBorder = new QCheckBox(tab);
        cbPinNoBorder->setObjectName(QStringLiteral("cbPinNoBorder"));

        horizontalLayout_2->addWidget(cbPinNoBorder);

        horizontalSpacer = new QSpacerItem(313, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout_2->addLayout(horizontalLayout_2);

        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        pbPinStatus = new QPushButton(tab);
        pbPinStatus->setObjectName(QStringLiteral("pbPinStatus"));

        gridLayout->addWidget(pbPinStatus, 1, 2, 1, 1);

        label_2 = new QLabel(tab);
        label_2->setObjectName(QStringLiteral("label_2"));

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        lePin = new QLineEdit(tab);
        lePin->setObjectName(QStringLiteral("lePin"));
        lePin->setMaximumSize(QSize(200, 16777215));
        lePin->setReadOnly(true);

        gridLayout->addWidget(lePin, 1, 1, 1, 1);

        label = new QLabel(tab);
        label->setObjectName(QStringLiteral("label"));

        gridLayout->addWidget(label, 0, 0, 1, 1);

        leScreenshot = new QLineEdit(tab);
        leScreenshot->setObjectName(QStringLiteral("leScreenshot"));
        leScreenshot->setMaximumSize(QSize(200, 16777215));
        leScreenshot->setReadOnly(true);

        gridLayout->addWidget(leScreenshot, 0, 1, 1, 1);

        pbScreenshotStatus = new QPushButton(tab);
        pbScreenshotStatus->setObjectName(QStringLiteral("pbScreenshotStatus"));

        gridLayout->addWidget(pbScreenshotStatus, 0, 2, 1, 1);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_4, 0, 3, 1, 1);

        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_5, 1, 3, 1, 1);

        gridLayout->setColumnStretch(1, 1);

        verticalLayout_2->addLayout(gridLayout);

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

        tabWidget->addTab(tab_3, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QStringLiteral("tab_2"));
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
        cbPinNoBorder->setText(QApplication::translate("Settings", "\350\264\264\345\233\276\346\227\240\350\276\271\346\241\206", Q_NULLPTR));
        pbPinStatus->setText(QString());
        label_2->setText(QApplication::translate("Settings", "\350\264\264\345\233\276\345\277\253\346\215\267\351\224\256\357\274\232", Q_NULLPTR));
        lePin->setPlaceholderText(QApplication::translate("Settings", "\347\202\271\345\207\273\351\234\200\350\246\201\350\256\276\347\275\256\347\232\204\345\277\253\346\215\267\351\224\256", Q_NULLPTR));
        label->setText(QApplication::translate("Settings", "\346\210\252\345\233\276\345\277\253\346\215\267\351\224\256\357\274\232", Q_NULLPTR));
        leScreenshot->setPlaceholderText(QApplication::translate("Settings", "\347\202\271\345\207\273\351\234\200\350\246\201\350\256\276\347\275\256\347\232\204\345\277\253\346\215\267\351\224\256", Q_NULLPTR));
        pbScreenshotStatus->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(tab), QApplication::translate("Settings", "\345\270\270\350\247\204\350\256\276\347\275\256", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tab_3), QApplication::translate("Settings", "\350\267\257\345\276\204", Q_NULLPTR));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QApplication::translate("Settings", "\345\205\263\344\272\216", Q_NULLPTR));
        pbRevert->setText(QApplication::translate("Settings", "\346\201\242\345\244\215\351\273\230\350\256\244", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class Settings: public Ui_Settings {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGS_H
