#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <functional>

// Separates the delayed fill sequence from clipboard and Windows UI access.
class FavoriteAccountFillController : public QObject {
public:
    struct Actions {
        std::function<bool()> targetActive;
        std::function<bool()> canEnterPassword;
        std::function<bool()> tab;
        std::function<bool(const QString&)> pastePassword;
        std::function<void()> restoreClipboard;
    };

    explicit FavoriteAccountFillController(Actions actions, QObject* parent = nullptr);
    bool Reserve();
    bool Busy() const { return stage_ != Stage::Idle; }
    void UsernamePasted(QString password);
    void Cancel();

private:
    enum class Stage { Idle, Reserved, WaitBeforeTab, CheckInput, Restore };
    void Advance();
    void Finish();
    void Wait(int milliseconds);

    Actions actions_;
    QTimer timer_;
    QElapsedTimer waitElapsed_;
    int minimumWaitMs_ = 0;
    Stage stage_ = Stage::Idle;
    QString password_;
    int tabAttempts_ = 0;
};
