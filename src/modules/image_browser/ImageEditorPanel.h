#pragma once

#include "ImageDocument.h"
#include <QFrame>
#include <QIcon>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace ImageBrowser {

// Owns editor controls and their drafts. The window owns the document and history.
class ImageEditorPanel final : public QFrame {
    Q_OBJECT
public:
    explicit ImageEditorPanel(QWidget* parent = nullptr);
    static QIcon editIcon();
    void setMetrics(double scale, int availableHeight);
    void setDocumentContext(const QString& path, QSize size, bool editable, bool colorBusy = false);
    void setSelectionState(bool selecting, const QRect& rect);
    // 后台图像处理期间在面板内提示并锁住按钮，否则用户会以为点击没有生效。
    void setBusy(bool busy);
    EditOperation currentOperation() const;
    bool drawingActive() const { return (currentTool_ == 1 || currentTool_ == 3) && editable_; }
    bool validateOperation(const EditOperation& operation);
    void resetColorParameters();

signals:
    void closeRequested();
    void toolChanged();
    void drawingStyleChanged();
    void colorChanged(const ImageBrowser::EditOperation& operation);
    void editRequested(const ImageBrowser::EditOperation& operation, bool selectRegion);
    void applySelectionRequested();
    void cancelSelectionRequested();

private:
    QWidget* createPage(const QString& title, const QString& description);
    QSpinBox* addSpin(QWidget* page, const QString& label, const QString& name,
                      int minimum, int maximum, int value);
    QLineEdit* addColor(QWidget* page, const QString& name);
    void selectTool(int tool);
    void submit();
    void syncButtons();
    void showMessage(const QString& text);

    QButtonGroup* tools_;
    QWidget* toolGrid_;
    QStackedWidget* pages_;
    QScrollArea* scroll_;
    QLabel* message_;
    QPushButton* submit_;
    QPushButton* applySelection_;
    QPushButton* cancelSelection_;
    QButtonGroup* annotationShape_;
    QComboBox* filter_;
    QComboBox* watermarkMode_;
    QButtonGroup* mosaicStyle_;
    QSpinBox* mosaicBrush_;
    QLineEdit* mosaicColor_;
    QSpinBox *filterStrength_, *cutoutIterations_, *cutoutFeather_;
    QSpinBox *watermarkOpacity_, *watermarkMargin_, *temperature_, *tint_, *opacity_;
    QLineEdit *annotationText_, *watermarkText_, *annotationColor_, *watermarkColor_, *assetPath_;
    QSpinBox *annotationWidth_, *annotationFont_, *watermarkFont_, *mosaicBlock_;
    QSpinBox *brightness_, *contrast_, *saturation_, *resizeWidth_, *resizeHeight_;
    QCheckBox* aspectLock_;
    QList<QSpinBox*> corners_;
    QSize imageSize_;
    QString imagePath_;
    int currentTool_ = 0;
    bool editable_ = false;
    bool busy_ = false;
    bool processing_ = false;
    bool selecting_ = false;
    bool validSelection_ = false;
    double lastScale_ = -1;
    int lastHeight_ = -1;
    QString lastStyleKey_;
};
} // namespace ImageBrowser
