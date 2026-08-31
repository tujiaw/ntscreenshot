#pragma once

#include <QObject>
#include <QStringList>

#include <atomic>
#include <memory>
#include <vector>

class QThread;

class DirectoryChangeWatcher final : public QObject {
    Q_OBJECT
public:
    explicit DirectoryChangeWatcher(QObject* parent = nullptr);
    ~DirectoryChangeWatcher() override;

    void start(const QStringList& roots);
    void stop();

signals:
    void rootChanged(const QString& root);
    void rootUnavailable(const QString& root);

private:
    struct Worker;
    std::vector<std::unique_ptr<Worker>> workers_;
};
