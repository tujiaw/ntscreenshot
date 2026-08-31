#pragma once

#include <memory>
#include <QRect>
#include <QWidget>
#include <functional>
#include <vector>

#include <map>
#include <set>
#include <mutex>

class QPixmap;

// --- 定义探测器策略接口 ---
class IRectDetector {
public:
    virtual ~IRectDetector() = default;
    
    // 初始化探测器，onReady 为异步处理完成后的回调
    virtual void init(std::shared_ptr<QPixmap> originScreen, std::function<void()> onReady) {}
    
    // 根据鼠标坐标探测矩形
    virtual QRect detectRect(const QPoint& cursorPos, QWidget* window) = 0;
    
    // 重置内部状态
    virtual void reset() {}
};

// --- 系统窗口探测器 ---
class SystemWindowDetector : public IRectDetector {
public:
    void init(std::shared_ptr<QPixmap> originScreen, std::function<void()> onReady) override;
    QRect detectRect(const QPoint& cursorPos, QWidget* window) override;
};

// --- OpenCV智能吸附探测器 ---
class OpenCVDetector : public IRectDetector {
public:
    void init(std::shared_ptr<QPixmap> originScreen, std::function<void()> onReady) override;
    QRect detectRect(const QPoint& cursorPos, QWidget* window) override;
    void reset() override;

private:
    std::shared_ptr<QPixmap> originScreen_;
    
    struct QRectCompare {
        bool operator()(const QRect& a, const QRect& b) const {
            if (a.x() != b.x()) return a.x() < b.x();
            if (a.y() != b.y()) return a.y() < b.y();
            if (a.width() != b.width()) return a.width() < b.width();
            return a.height() < b.height();
        }
    };
    
    std::map<QRect, std::vector<QRect>, QRectCompare> cachedWindowRects_;
    std::set<QRect, QRectCompare> processingWindows_;
    std::mutex cacheMutex_;
};