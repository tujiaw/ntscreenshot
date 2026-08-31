#pragma once

#include "AiFillSettings.h"

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>

// Qt port of wtl_clipboard's AiFillSettingsDialog. Edits an AiFillSettings
// struct in place; the caller persists it via ClipboardStore on accept.
class AiFillSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AiFillSettingsDialog(AiFillSettings& settings, QWidget* parent = nullptr);

private slots:
    void onAccept();

private:
    AiFillSettings& settings_;
    QLineEdit* endpointEdit_ = nullptr;
    QLineEdit* apiKeyEdit_ = nullptr;
    QLineEdit* modelEdit_ = nullptr;
    QCheckBox* enabledCheck_ = nullptr;
};
