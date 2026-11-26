#pragma once

// Small helpers shared by the Settings translation units. They stay header-only
// so panel files remain self-contained without exporting internal symbols from
// the module's only public header (Settings.h).

#include <QIcon>
#include <QKeySequence>
#include <QPushButton>
#include <QString>

namespace settings_detail {

inline QString keySequenceText(const QKeySequence& sequence)
{
    return sequence.toString(QKeySequence::NativeText).trimmed();
}

inline void setStatusIcon(QPushButton* button, bool ok)
{
    button->setIcon(QIcon(ok ? ":/images/ok.png" : ":/images/remove.png"));
}

}  // namespace settings_detail
