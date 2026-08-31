#pragma once

#include <QObject>
#include <QHash>
#include <QString>

#include <functional>

class QHotkey;

class GlobalHotkeyRegistry : public QObject
{
public:
    explicit GlobalHotkeyRegistry(QObject *parent = nullptr);
    ~GlobalHotkeyRegistry() override;

    bool setHotkey(const QString &actionId, const QString &key, const std::function<void()> &callback);
    void clearHotkey(const QString &actionId);
    QString hotkey(const QString &actionId) const;

    //! Reason the most recent setHotkey() call failed (empty on success).
    QString lastError() const { return lastError_; }

private:
    QHash<QString, QHotkey *> shortcuts_;
    QHash<QString, QString> keys_;
    QHash<QString, std::function<void()>> callbacks_;
    QString lastError_;
};
