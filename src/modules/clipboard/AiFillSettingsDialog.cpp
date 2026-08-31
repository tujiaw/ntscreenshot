#include "AiFillSettingsDialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

AiFillSettingsDialog::AiFillSettingsDialog(AiFillSettings& settings, QWidget* parent)
    : QDialog(parent), settings_(settings) {
    setWindowTitle(QStringLiteral("AI Fill Settings"));
    setMinimumWidth(380);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* form = new QFormLayout();
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    endpointEdit_ = new QLineEdit(this);
    endpointEdit_->setText(settings_.apiEndpoint);
    form->addRow(QStringLiteral("API Endpoint"), endpointEdit_);

    apiKeyEdit_ = new QLineEdit(this);
    apiKeyEdit_->setEchoMode(QLineEdit::Password);
    apiKeyEdit_->setText(settings_.apiKey);
    form->addRow(QStringLiteral("API Key"), apiKeyEdit_);

    modelEdit_ = new QLineEdit(this);
    modelEdit_->setText(settings_.modelName);
    form->addRow(QStringLiteral("Model"), modelEdit_);

    root->addLayout(form);

    enabledCheck_ = new QCheckBox(QStringLiteral("Enable AI Fill"), this);
    enabledCheck_->setChecked(settings_.enabled);
    root->addWidget(enabledCheck_);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto* okButton = new QPushButton(QStringLiteral("OK"), this);
    okButton->setDefault(true);
    auto* cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    buttonRow->addWidget(okButton);
    buttonRow->addWidget(cancelButton);
    root->addLayout(buttonRow);

    connect(okButton, &QPushButton::clicked, this, &AiFillSettingsDialog::onAccept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void AiFillSettingsDialog::onAccept() {
    settings_.apiEndpoint = endpointEdit_->text().trimmed();
    settings_.apiKey = apiKeyEdit_->text();
    settings_.modelName = modelEdit_->text().trimmed();
    settings_.enabled = enabledCheck_->isChecked();
    accept();
}
