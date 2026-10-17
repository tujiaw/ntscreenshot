#pragma once
#include <QWidget>

class FavoriteSyncController;
class QLabel;
class QPushButton;
class QGroupBox;
class QScrollArea;

class FavoriteSyncPage final : public QWidget {
    Q_OBJECT
public:
    explicit FavoriteSyncPage(FavoriteSyncController* controller, QWidget* parent = nullptr);
private:
    void Refresh();
    FavoriteSyncController* controller_;
    QLabel *account_, *status_, *details_, *code_;
    QPushButton *signIn_, *sync_, *signOut_, *copyCode_, *openBrowser_;
    QGroupBox* verification_;
    QScrollArea* scrollArea_;
    int displayMode_ = -1;
};
