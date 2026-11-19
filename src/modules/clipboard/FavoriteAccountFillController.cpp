#include "FavoriteAccountFillController.h"

#include <utility>

FavoriteAccountFillController::FavoriteAccountFillController(Actions actions, QObject* parent)
    : QObject(parent), actions_(std::move(actions)) {
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &FavoriteAccountFillController::Advance);
}

bool FavoriteAccountFillController::Reserve(Mode mode, int terminalDelayMs) {
    if (Busy()) return false;
    mode_ = mode;
    terminalDelayMs_ = qBound(100, terminalDelayMs, 10000);
    stage_ = Stage::Reserved;
    tabAttempts_ = 0;
    return true;
}

void FavoriteAccountFillController::UsernamePasted(QString password) {
    if (stage_ != Stage::Reserved) return;
    password_ = std::move(password);
    stage_ = mode_ == Mode::SshTerminal ? Stage::WaitBeforeEnter : Stage::WaitBeforeTab;
    Wait(100);
}

void FavoriteAccountFillController::Wait(int milliseconds) {
    minimumWaitMs_ = milliseconds;
    waitElapsed_.start();
    timer_.start(milliseconds);
}

void FavoriteAccountFillController::Finish() {
    password_.clear();
    stage_ = Stage::Restore;
    // Give the destination time to consume the clipboard before restoring it.
    Wait(300);
}

void FavoriteAccountFillController::Cancel() {
    timer_.stop();
    password_.clear();
    if (Busy()) actions_.restoreClipboard();
    stage_ = Stage::Idle;
}

void FavoriteAccountFillController::Advance() {
    // Some Windows timer backends round to the nearest millisecond.
    const auto remaining = minimumWaitMs_ - waitElapsed_.elapsed();
    if (remaining > 0) {
        timer_.start(static_cast<int>(remaining));
        return;
    }
    if (stage_ == Stage::Restore) {
        Cancel();
        return;
    }
    if (!actions_.targetActive() || password_.isEmpty()) {
        Finish();
        return;
    }
    if (stage_ == Stage::WaitBeforeEnter) {
        if (!actions_.enter || !actions_.enter()) { Finish(); return; }
        stage_ = Stage::WaitPassword;
        Wait(terminalDelayMs_);
        return;
    }
    if (stage_ == Stage::WaitPassword) {
        if (!actions_.pastePassword(password_)) { Finish(); return; }
        // Let the terminal process the paste before submitting the password.
        stage_ = Stage::SubmitPassword;
        Wait(100);
        return;
    }
    if (stage_ == Stage::SubmitPassword) {
        if (actions_.enter) actions_.enter();
        Finish();
        return;
    }
    if (stage_ == Stage::CheckInput && actions_.canEnterPassword()) {
        // UI inspection can take time. Check the destination again before writing.
        if (actions_.targetActive()) actions_.pastePassword(password_);
        Finish();
        return;
    }
    if (tabAttempts_ >= 2 || !actions_.targetActive() || !actions_.tab()) {
        Finish();
        return;
    }
    ++tabAttempts_;
    stage_ = Stage::CheckInput;
    // Tab is asynchronous; inspect focus only after the destination processes it.
    Wait(50);
}
