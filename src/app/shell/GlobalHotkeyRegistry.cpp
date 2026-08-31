#include "app/shell/GlobalHotkeyRegistry.h"

#include "qhotkey.h"

#include <QDebug>
#include <QKeySequence>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

GlobalHotkeyRegistry::GlobalHotkeyRegistry(QObject *parent)
    : QObject(parent)
{
}

GlobalHotkeyRegistry::~GlobalHotkeyRegistry()
{
    const auto ids = shortcuts_.keys();
    for (const QString &actionId : ids) {
        clearHotkey(actionId);
    }
}

bool GlobalHotkeyRegistry::setHotkey(const QString &actionId, const QString &key, const std::function<void()> &callback)
{
    const QString normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        clearHotkey(actionId);
        callbacks_.remove(actionId);
        return true;
    }

    if (keys_.value(actionId) == normalizedKey) {
        callbacks_.insert(actionId, callback);
        return true;
    }

    const QKeySequence sequence = QKeySequence::fromString(normalizedKey, QKeySequence::NativeText);
    if (sequence.isEmpty()) {
        return false;
    }

    auto *shortcut = new QHotkey(sequence, false, this);
    if (!shortcut->setRegistered(true)) {
        lastError_ = shortcut->errorString();
        if (lastError_.isEmpty()) {
            lastError_ = QStringLiteral("快捷键注册失败 (错误码 %1)")
                             .arg(::GetLastError());
        }
        qWarning() << "GlobalHotkeyRegistry: failed to register hotkey" << normalizedKey
                   << "-" << lastError_;
        delete shortcut;
        return false;
    }

    clearHotkey(actionId);
    shortcuts_.insert(actionId, shortcut);
    keys_.insert(actionId, normalizedKey);
    callbacks_.insert(actionId, callback);
    lastError_.clear();
    connect(shortcut, &QHotkey::activated, this, [this, actionId]() {
        const auto callback = callbacks_.value(actionId);
        if (callback) {
            callback();
        }
    });
    return true;
}

void GlobalHotkeyRegistry::clearHotkey(const QString &actionId)
{
    if (QHotkey *shortcut = shortcuts_.take(actionId)) {
        delete shortcut;
    }
    keys_.remove(actionId);
    callbacks_.remove(actionId);
}

QString GlobalHotkeyRegistry::hotkey(const QString &actionId) const
{
    return keys_.value(actionId);
}
