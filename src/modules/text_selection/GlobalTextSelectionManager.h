#pragma once

#include <memory>
#include <QObject>
#include <QPoint>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class SettingModel;

class TextSelectionToolbar;

class GlobalTextSelectionManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalTextSelectionManager(QObject *parent = nullptr);
    ~GlobalTextSelectionManager() override;

    void setSettingModel(SettingModel *setting);

signals:
    void sigActionTriggered(const QString &actionId, const QString &selectedText, const QString &inputText);

private:
#ifdef Q_OS_WIN
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    void handleGlobalMouseEvent(WPARAM wParam, const QPoint &globalPos);
    void triggerForSelection(const QPoint &globalPos, HWND sourceWindow);
    bool installMouseHook();
    void uninstallMouseHook();
    bool popupContainsGlobalPoint(const QPoint &globalPos) const;
    bool isOwnProcessWindow(HWND hwnd) const;
    QString captureSelectedText(HWND sourceWindow);
    QString captureSelectedTextWithRetry(HWND sourceWindow);

    HHOOK mouseHook_ = nullptr;
    QPoint pressPoint_;
    QPoint lastClickPoint_;
    HWND dragSourceWindow_ = nullptr;
    bool dragging_ = false;
    bool doubleClickCandidate_ = false;
    DWORD lastClickTime_ = 0;
#endif

    void hidePopup();
    void showPopup(const QPoint &globalPos, const QString &selectedText);

    QString selectedText_;
    std::unique_ptr<TextSelectionToolbar> popup_;
    SettingModel *setting_ = nullptr;
};
