#include "modules/clipboard/FavoriteAccountFillController.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    // First input, second input, no writable input, destination lost, Tab failure,
    // clipboard failure, and a destination change during focus inspection.
    for (int scenario = 0; scenario < 8; ++scenario) {
        int tabs = 0, checks = 0, pastes = 0, restores = 0;
        bool targetActive = scenario != 3;
        bool correctPassword = false;
        qint64 firstTabAt = -1, lastPasteAt = -1, restoredAt = -1;
        QElapsedTimer elapsed;
        FavoriteAccountFillController fill({
            [&]() { return targetActive; },
            [&]() {
                ++checks;
                if (scenario == 7) targetActive = false;
                return scenario == 0 || scenario == 5 || scenario == 7 ||
                       (scenario == 1 && tabs == 2);
            },
            [&]() {
                ++tabs;
                if (tabs == 1) firstTabAt = elapsed.elapsed();
                if (scenario == 4) targetActive = false;
                return scenario != 6;
            },
            [&](const QString& password) {
                ++pastes;
                correctPassword = password == QStringLiteral("secret-password");
                lastPasteAt = elapsed.elapsed();
                return scenario != 5;
            },
            [&]() { ++restores; restoredAt = elapsed.elapsed(); }
        });
        CHECK(fill.Reserve());
        CHECK(!fill.Reserve()); // Double-clicks cannot restart or overlap a fill.
        elapsed.start();
        fill.UsernamePasted(QStringLiteral("secret-password"));
        while (fill.Busy() && elapsed.elapsed() < 2000)
            app.processEvents(QEventLoop::AllEvents, 10);
        CHECK(!fill.Busy() && restores == 1);
        CHECK(firstTabAt < 0 || firstTabAt >= 100);
        CHECK(lastPasteAt < 0 || restoredAt - lastPasteAt >= 300);
        if (scenario == 0 || scenario == 5) {
            CHECK(tabs == 1 && checks == 1 && pastes == 1 && correctPassword);
        } else if (scenario == 1) {
            CHECK(tabs == 2 && checks == 2 && pastes == 1 && correctPassword);
        } else if (scenario == 2) {
            CHECK(tabs == 2 && checks == 2 && pastes == 0);
        } else if (scenario == 3) {
            CHECK(tabs == 0 && checks == 0 && pastes == 0);
        } else if (scenario == 7) {
            CHECK(tabs == 1 && checks == 1 && pastes == 0);
        } else {
            CHECK(tabs == 1 && checks == 0 && pastes == 0);
        }
        fill.Cancel();
        CHECK(restores == 1);
        CHECK(fill.Reserve()); // Can be used again after cleanup.
        fill.UsernamePasted(QStringLiteral("cancelled-password"));
        fill.Cancel();
        CHECK(!fill.Busy() && restores == 2);
    }
    for (int scenario = 0; scenario < 7; ++scenario) {
        int enters = 0, tabs = 0, inspections = 0, pastes = 0, restores = 0;
        bool active = true;
        qint64 enterAt = -1, pasteAt = -1, submitAt = -1;
        QElapsedTimer elapsed; elapsed.start();
        FavoriteAccountFillController fill({
            [&]() { return active; },
            [&]() { ++inspections; return false; },
            [&]() { ++tabs; return true; },
            [&](const QString& password) {
                ++pastes; pasteAt = elapsed.elapsed();
                if (scenario == 5) active = false;
                return password == QStringLiteral("ssh-password") && scenario != 4;
            },
            [&]() { ++restores; },
            [&]() {
                ++enters;
                if (enters == 1) enterAt = elapsed.elapsed();
                else submitAt = elapsed.elapsed();
                return scenario != 2;
            }
        });
        CHECK(fill.Reserve(FavoriteAccountFillController::Mode::SshTerminal, 250));
        fill.UsernamePasted(QStringLiteral("ssh-password"));
        // Lose focus or cancel after Enter but before the password is pasted.
        if (scenario == 1) QTimer::singleShot(180, &fill, [&]() { active = false; });
        if (scenario == 3) QTimer::singleShot(180, &fill, [&]() { fill.Cancel(); });
        if (scenario == 6) QTimer::singleShot(400, &fill, [&]() { fill.Cancel(); });
        while (fill.Busy() && elapsed.elapsed() < 2000)
            app.processEvents(QEventLoop::AllEvents, 10);
        CHECK(!fill.Busy() && restores == 1);
        CHECK(enters == (scenario == 0 ? 2 : 1) && tabs == 0 && inspections == 0);
        CHECK(enterAt >= 100);
        if (scenario == 0 || scenario >= 4) CHECK(pastes == 1 && pasteAt - enterAt >= 250);
        else CHECK(pastes == 0);
        if (scenario == 0) CHECK(submitAt - pasteAt >= 100);
        else CHECK(submitAt == -1);
    }
    return 0;
}
