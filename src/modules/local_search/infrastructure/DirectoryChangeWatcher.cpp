#include "modules/local_search/infrastructure/DirectoryChangeWatcher.h"

#include <QDebug>
#include <QMetaObject>
#include <QThread>
#include <QDir>

#include <array>
#include <cstddef>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

struct DirectoryChangeWatcher::Worker {
    QString root;
    QThread* thread = nullptr;
    std::atomic_bool stopping{false};
#ifdef Q_OS_WIN
    std::atomic<HANDLE> handle{INVALID_HANDLE_VALUE};
#endif
};

DirectoryChangeWatcher::DirectoryChangeWatcher(QObject* parent)
    : QObject(parent)
{
}

DirectoryChangeWatcher::~DirectoryChangeWatcher()
{
    stop();
}

void DirectoryChangeWatcher::start(const QStringList& roots)
{
    qInfo() << "DirectoryChangeWatcher::start: watching" << roots.size() << "roots:" << roots;
    stop();
#ifdef Q_OS_WIN
    for (const QString& root : roots) {
        auto worker = std::make_unique<Worker>();
        worker->root = root;
        Worker* raw = worker.get();
        raw->thread = QThread::create([this, raw]() {
            qInfo() << "DirectoryChangeWatcher: worker thread started for" << raw->root;
            const std::wstring nativeRoot = QDir::toNativeSeparators(raw->root).toStdWString();
            HANDLE handle = CreateFileW(nativeRoot.c_str(), FILE_LIST_DIRECTORY,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING,
                                        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            raw->handle.store(handle);
            if (handle == INVALID_HANDLE_VALUE) {
                qWarning() << "DirectoryChangeWatcher: failed to open directory handle for" << raw->root;
                QMetaObject::invokeMethod(this, [this, root = raw->root]() {
                    emit rootUnavailable(root);
                }, Qt::QueuedConnection);
                return;
            }

            alignas(DWORD) std::array<std::byte, 64 * 1024> buffer{};
            while (!raw->stopping.load()) {
                DWORD bytesReturned = 0;
                const BOOL ok = ReadDirectoryChangesW(
                    handle, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
                    &bytesReturned, nullptr, nullptr);
                if (raw->stopping.load()) break;
                if (!ok || bytesReturned == 0) {
                    QMetaObject::invokeMethod(this, [this, root = raw->root]() {
                        emit rootUnavailable(root);
                    }, Qt::QueuedConnection);
                    break;
                }
                QMetaObject::invokeMethod(this, [this, root = raw->root]() {
                    emit rootChanged(root);
                }, Qt::QueuedConnection);
            }
            if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            raw->handle.store(INVALID_HANDLE_VALUE);
        });
        raw->thread->start();
        workers_.push_back(std::move(worker));
    }
#else
    Q_UNUSED(roots);
#endif
}

void DirectoryChangeWatcher::stop()
{
    if (workers_.empty()) return;
    qInfo() << "DirectoryChangeWatcher::stop: stopping" << workers_.size() << "workers...";
    for (auto& worker : workers_) {
        worker->stopping.store(true);
#ifdef Q_OS_WIN
        const HANDLE handle = worker->handle.load();
        if (handle != INVALID_HANDLE_VALUE) CancelIoEx(handle, nullptr);
#endif
    }
    for (auto& worker : workers_) {
        if (worker->thread) {
            worker->thread->quit();
            worker->thread->wait();
            delete worker->thread;
            worker->thread = nullptr;
        }
    }
    workers_.clear();
}
