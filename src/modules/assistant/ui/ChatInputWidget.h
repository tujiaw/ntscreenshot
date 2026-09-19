#pragma once

#include <QList>
#include <QPixmap>
#include <QString>
#include <QWidget>

class QEvent;
class QHBoxLayout;
class QPushButton;
class QToolButton;
class QWidget;
class QPlainTextEdit;
class SettingModel;

class ChatInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatInputWidget(SettingModel* settings, QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);
    void setPending(bool pending);
    bool webEnabled() const;
    void setWebEnabled(bool enabled);
    bool browserEnabled() const;
    void setBrowserEnabled(bool enabled);
    void quoteText(const QString &text);
    void quoteImage(const QPixmap &image, const QString &displayName = QStringLiteral("image.png"));
    void clearQuotedImage();

signals:
    void sigSendRequested(const QString &text, const QList<QPixmap> &images);
    void sigStopRequested();
    void sigClearRequested();
    void sigWebEnabledChanged(bool enabled);
    void sigBrowserEnabledChanged(bool enabled);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onAddButtonClicked();
    void onSendClicked();
    void showModelMenu();

private:
    SettingModel* settings_ = nullptr;
    struct QuotedReferenceItem {
        enum class Type {
            Text,
            Image
        };

        Type type = Type::Text;
        QString text;
        QString displayName;
        QPixmap image;
    };

    static constexpr int kMaxQuotedReferences = 3;

    void appendQuotedReference(const QuotedReferenceItem &item);
    void previewQuotedReference(int index);
    void updateSendButtonState();
    void refreshModelButton();
    // 联网 / 浏览器两个图标开关：按勾选状态换图标配色和提示文案。
    void refreshToggleButtons();
    void rebuildQuotedAttachmentsUi();
    void clearQuotedReferences();
    void adjustInputHeight();
    QString trimmedInputText() const;

    QWidget *quoteContainer_;
    QHBoxLayout *quoteLayout_;
    QPlainTextEdit *inputEdit_;
    QToolButton *addButton_;
    QToolButton *modelButton_;
    QToolButton *webButton_ = nullptr;
    QToolButton *browserButton_ = nullptr;
    QToolButton *clearButton_ = nullptr;
    QPushButton *sendButton_;
    QList<QuotedReferenceItem> quotedReferences_;
    bool pending_;
};
