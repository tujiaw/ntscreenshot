#pragma once

#include <QAtomicInt>
#include <QDebug>
#include <QObject>
#include <QThread>
#include <functional>

// Runs work() on a dedicated worker thread, then invokes onDone() on the
// thread that owns this object (normally the main/UI thread).
class AsyncRunner : public QObject {
    Q_OBJECT

public:
    explicit AsyncRunner(QObject* parent = nullptr)
        : QObject(parent) {}

    ~AsyncRunner() override {
        cancel();
    }

    AsyncRunner(const AsyncRunner&) = delete;
    AsyncRunner& operator=(const AsyncRunner&) = delete;

    bool busy() const {
        return running_.loadAcquire();
    }

    // Returns false if a task is already running.
    bool start(std::function<void()> work, std::function<void()> onDone) {
        if (running_.loadAcquire()) {
            qWarning() << "AsyncRunner::start: already busy, rejecting";
            return false;
        }
        qInfo() << "AsyncRunner::start: launching worker thread";
        work_ = std::move(work);
        onDone_ = std::move(onDone);
        running_.storeRelease(true);
        cancelled_ = false;

        QThread* worker = QThread::create([this]() {
            if (work_) {
                work_();
            }
            QMetaObject::invokeMethod(this, "onFinished", Qt::QueuedConnection);
        });
        thread_ = worker;
        connect(worker, &QThread::finished, this, [this, worker]() {
            if (thread_ == worker) {
                thread_ = nullptr;
            }
            worker->deleteLater();
        });
        worker->start();
        return true;
    }

    // Cancel the running task. Blocks until the worker thread exits.
    void cancel() {
        qInfo() << "AsyncRunner::cancel: cancelling worker thread";
        cancelled_ = true;
        running_.storeRelease(false);
        if (thread_) {
            QThread* worker = thread_;
            worker->requestInterruption();
            worker->quit();
            worker->wait();
            worker->deleteLater();
            thread_ = nullptr;
        }
    }

public slots:
    void onFinished() {
        qInfo() << "AsyncRunner::onFinished: worker done, cancelled =" << cancelled_;
        std::function<void()> callback = std::move(onDone_);
        onDone_ = nullptr;
        work_ = nullptr;
        running_.storeRelease(false);
        if (callback && !cancelled_) {
            callback();
        }
    }

private:
    QThread* thread_ = nullptr;
    std::function<void()> work_;
    std::function<void()> onDone_;
    QAtomicInt running_{0};
    bool cancelled_ = false;
};
