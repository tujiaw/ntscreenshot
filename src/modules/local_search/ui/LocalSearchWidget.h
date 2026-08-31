#pragma once

#include "modules/local_search/domain/SearchTypes.h"

#include <QFutureWatcher>
#include <QHash>
#include <QIcon>
#include <QSet>
#include <QWidget>

#include <memory>

class SearchAggregator;
class SearchIndexService;
class SearchIndexStore;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QTimer;
class QNetworkAccessManager;
class QUrl;
class QPaintEvent;
class QVBoxLayout;

class LocalSearchWidget final : public QWidget {
    Q_OBJECT
public:
    LocalSearchWidget(std::shared_ptr<SearchAggregator> aggregator,
                      std::shared_ptr<SearchIndexStore> store,
                      SearchIndexService* service,
                      QWidget* parent = nullptr);

    void activateSearch();
    void setScaleFactor(qreal scaleFactor);
    void setAvailableWidth(int width);
    void setWebSearchEngine(const QString& baseUrl, const QString& queryParam);
    void setPinyinSearchEnabled(bool enabled);
    QSize compactWindowSize() const;

signals:
    void requestHide();
    void preferredHeightChanged(int height);
    void requestSetWebSearchEngine(const QString& engineId);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void startSearch();
    void showResults(const QVector<SearchResult>& results);
    QIcon iconForResult(const SearchResult& result);
    QIcon bookmarkIcon(const SearchResult& result);
    QIcon browserIconForBookmark(const SearchResult& result);
    void requestFavicon(const QUrl& pageUrl, const QString& domain);
    void requestFaviconImage(const QUrl& iconUrl, const QUrl& fallbackUrl,
                             const QString& domain, bool allowFallback);
    void applyFavicon(const QString& domain, const QPixmap& pixmap);
    void openCurrent();
    void locateCurrent();
    void copyCurrentPath();
    bool runCurrentAsAdmin();
    bool openCurrentInTerminal();
    void openCurrentEngineHome();
    void copyCurrentEngineUrl();
    void setCurrentEngineDefault();
    void showSearchEngines(const QString& query);
    void showContextMenu();
    void hideContextMenu();
    void executeContextAction(int actionId);
    void moveSelection(int offset);
    void addContextAction(const QString& text, const QIcon& icon, int actionId);
    void setResultsVisible(bool visible);
    int compactHeight() const;
    int expandedHeight() const;
    int contextMenuHeight() const;
    int scaled(int value) const;
    const SearchResult* currentResult() const;

    std::shared_ptr<SearchAggregator> aggregator_;
    std::shared_ptr<SearchIndexStore> store_;
    SearchIndexService* service_ = nullptr;
    QFutureWatcher<QVector<SearchResult>>* searchWatcher_ = nullptr;
    QTimer* searchTimer_ = nullptr;
    QLineEdit* query_ = nullptr;
    QListWidget* results_ = nullptr;
    QLabel* resultCount_ = nullptr;
    QLabel* footerHint_ = nullptr;
    QStackedWidget* stackedWidget_ = nullptr;
    QListWidget* contextMenuList_ = nullptr;
    QWidget* resultsPanel_ = nullptr;
    QVBoxLayout* rootLayout_ = nullptr;
    QVBoxLayout* resultsLayout_ = nullptr;
    QVector<SearchResult> currentResults_;
    QHash<QString, QIcon> iconCache_;
    QHash<QString, QIcon> faviconCache_;
    QSet<QString> pendingFavicons_;
    QNetworkAccessManager* network_ = nullptr;
    QString faviconCacheDirectory_;
    QString webSearchUrl_ = QStringLiteral("https://www.bing.com/search");
    QString webSearchQueryParam_ = QStringLiteral("q");
    QString pendingQuery_;
    bool rerunPending_ = false;
    bool resultsVisible_ = false;
    bool contextMenuVisible_ = false;
    bool showingEngines_ = false;
    bool pinyinEnabled_ = true;
    qreal scaleFactor_ = 1.0;
    int availableWidth_ = 0;
    int requestedWidth_ = 0;
};
