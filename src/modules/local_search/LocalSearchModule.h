#pragma once

#include "core/modules/IToolModule.h"

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>

#include <memory>

class LocalSearchWidget;
class SearchAggregator;
class SearchIndexService;
class SearchIndexStore;
class SettingModel;

class LocalSearchModule final : public QObject, public IToolModule {
    Q_OBJECT
public:
    explicit LocalSearchModule(SettingModel* settings);

    QString id() const override { return QStringLiteral("local_search"); }
    void initialize() override;
    void shutdown() override;

    void open();
    void applySettings();
    void rebuildIndex();
    QString statusText() const;

private:
    void updateWindowGeometry(int preferredHeight);

    SettingModel* settings_ = nullptr;
    std::shared_ptr<SearchIndexStore> store_;
    std::shared_ptr<SearchAggregator> aggregator_;
    SearchIndexService* service_ = nullptr;
    QPointer<LocalSearchWidget> window_;
    QPointer<LocalSearchWidget> widget_;
    QRect anchorArea_;
    QPoint anchorTopLeft_;
};
