/********************************************************************************
** Form generated from reading UI file 'ConfigUIwvxXft.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef CONFIGUIWVXXFT_H
#define CONFIGUIWVXXFT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>

QT_BEGIN_NAMESPACE

class Ui_ConfigDialog
{
public:
    QDialogButtonBox *buttonBox;
    QPlainTextEdit *ServerAddress;
    QLabel *label;
    QPlainTextEdit *ServerPort;
    QLabel *label_2;

    void setupUi(QDialog *ConfigDialog)
    {
        if (ConfigDialog->objectName().isEmpty())
            ConfigDialog->setObjectName(QString::fromUtf8("ConfigDialog"));
        ConfigDialog->setWindowModality(Qt::NonModal);
        ConfigDialog->resize(400, 153);
        buttonBox = new QDialogButtonBox(ConfigDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setGeometry(QRect(20, 110, 361, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        ServerAddress = new QPlainTextEdit(ConfigDialog);
        ServerAddress->setObjectName(QString::fromUtf8("ServerAddress"));
        ServerAddress->setGeometry(QRect(20, 30, 361, 21));
        ServerAddress->setAcceptDrops(true);
        ServerAddress->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ServerAddress->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        label = new QLabel(ConfigDialog);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(20, 10, 81, 16));
        ServerPort = new QPlainTextEdit(ConfigDialog);
        ServerPort->setObjectName(QString::fromUtf8("ServerPort"));
        ServerPort->setGeometry(QRect(20, 80, 71, 21));
        ServerPort->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ServerPort->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        label_2 = new QLabel(ConfigDialog);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(20, 60, 81, 16));

        retranslateUi(ConfigDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), ConfigDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), ConfigDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(ConfigDialog);
    } // setupUi

    void retranslateUi(QDialog *ConfigDialog)
    {
        ConfigDialog->setWindowTitle(QCoreApplication::translate("ConfigDialog", "CPP-PAR Configuration", nullptr));
        ServerAddress->setPlainText(QString());
        label->setText(QCoreApplication::translate("ConfigDialog", "Server Address", nullptr));
        label_2->setText(QCoreApplication::translate("ConfigDialog", "Port", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConfigDialog: public Ui_ConfigDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // CONFIGUIWVXXFT_H
