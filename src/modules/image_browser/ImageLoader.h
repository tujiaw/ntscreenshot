#pragma once

#include <QObject>
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

namespace ImageBrowser {

// Workers own their inputs. Watchers are owned by the receiving UI object, so
// closing a window disconnects completion without waiting for image decoding.
class ImageLoader final : public QObject {
public:
    explicit ImageLoader(QObject* parent = nullptr) : QObject(parent) {
        pool_.setMaxThreadCount(2);
        pool_.setExpiryTimeout(5000);
    }

    template<class Work, class Done>
    void run(QObject* receiver, Work work, Done done) {
        using Result = decltype(work());
        auto* watcher = new QFutureWatcher<Result>(receiver);
        QObject::connect(watcher, &QFutureWatcher<Result>::finished, receiver,
                         [watcher, done = std::move(done)]() mutable {
            Result result;
            try {
                result = watcher->result();
            } catch (...) {
                result.error = QCoreApplication::translate("ImageBrowser", "图片处理失败或内存不足。");
            }
            watcher->deleteLater();
            done(result);
        });
        watcher->setFuture(QtConcurrent::run(&pool_, std::move(work)));
    }

private:
    QThreadPool pool_;
};

} // namespace ImageBrowser
