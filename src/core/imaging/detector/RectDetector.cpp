#include "RectDetector.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QPixmap>

#include <opencv2/opencv.hpp>
#include "core/platform/Util.h"

// ==========================================
// 策略模式：探测器实现
// ==========================================

void SystemWindowDetector::init(std::shared_ptr<QPixmap> originScreen, std::function<void()> onReady)
{
    Util::cacheAllWindows();
    if (onReady) onReady();
}

QRect SystemWindowDetector::detectRect(const QPoint& cursorPos, QWidget* window)
{
    Q_UNUSED(cursorPos);
    if (!window) {
        return QRect();
    }

    QRect tmpRect;
    if (!Util::getRectFromCurrentPoint(window->winId(), tmpRect)) {
        return QRect();
    }

    return Util::clampToDesktopLocal(Util::toDesktopLocal(tmpRect));
}

// ==========================================
// OpenCV 智能吸附探测器 — 内部工具函数
// ==========================================

namespace {

// --- 1. 阈值计算 ---
std::pair<double, double> adaptiveCannyThresholds(const cv::Mat& /*gray*/)
{
    // 对于 UI 截图（极少有真实世界的传感器噪点），直接使用较低的固定阈值。
    // 原先基于图像亮度的自适应算法会导致在亮色（白底）主题下产生过高阈值（如 >240），
    // 从而直接漏掉极淡的 UI 边框、浅色分割线和轻微的背景色差。
    return { 15.0, 45.0 };
}

// CLAHE 局部对比度增强，改善低对比度区域的边缘可见性
// 【已移除】：屏幕截图不需要处理光照不均，CLAHE 会放大字体的抗锯齿边缘（亚像素），将其变成噪点。

// --- 2. 尺度边缘检测 ---
// 【已移除】：强制缩小一半（0.5）再用最邻近插值放大，直接破坏了 UI 界面“像素级（1px）”的精准度。
// 这会导致截图软件最后吸附的框总是偏大或偏小 1-2 个像素。

// --- 3. 多通道边缘检测：颜色空间各通道提供互补的边缘信息 ---
cv::Mat detectEdgesFromChannel(const cv::Mat& channel)
{
    auto [low, high] = adaptiveCannyThresholds(channel);
    cv::Mat edges;
    cv::Canny(channel, edges, low, high);
    return edges;
}

cv::Mat detectMultiChannelEdges(const cv::Mat& bgr, const cv::Mat& gray)
{
    // 直接在原尺寸的灰度图上进行高精度边缘检测
    cv::Mat edgeGray = detectEdgesFromChannel(gray);

    // HSV：H 通道捕捉颜色跳变，S 通道捕捉饱和度边界
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> hsvCh;
    cv::split(hsv, hsvCh);

    // 保留 S 通道，用于捕捉亮度相同但颜色不同的隐形边界
    cv::Mat edgeS = detectEdgesFromChannel(hsvCh[1]);

    cv::Mat combined;
    cv::bitwise_or(edgeGray, edgeS, combined);
    return combined;
}

// --- 4. 形态学后处理：智能连通 ---
cv::Mat morphPostProcess(const cv::Mat& edges)
{
    cv::Mat result;

    // 1. 轻微的膨胀，修复由于低对比度导致的 1px 虚线/边框断裂
    // 使用十字交叉核 (Cross) 而不是矩形核 (Rect)，避免边角产生斜向的不自然粘连
    cv::Mat dilateKernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));
    cv::dilate(edges, result, dilateKernel);

    // 2. 核心优化：使用“宽扁型”卷积核进行闭操作
    // 专门用于将同一行相邻的“文字（字母间距）”、“图标与文字”横向连接成一个完整的“行区块”，
    // 同时保证垂直方向的高度不会越界。
    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(12, 2));
    cv::morphologyEx(result, result, cv::MORPH_CLOSE, closeKernel);

    // 3. 腐蚀还原边缘的真实位置
    cv::Mat erodeKernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));
    cv::erode(result, result, erodeKernel);

    return result;
}

// --- 5. 高级轮廓过滤 ---

// IoU 重叠度检测，用于 NMS 去重
bool isHighOverlap(const QRect& a, const QRect& b, double threshold = 0.7)
{
    QRect inter = a.intersected(b);
    if (!inter.isValid()) return false;
    int interArea = inter.width() * inter.height();
    int unionArea = a.width() * a.height() + b.width() * b.height() - interArea;
    return unionArea > 0 && static_cast<double>(interArea) / unionArea >= threshold;
}

// 基于形状特征的轮廓过滤 + IoU-NMS 去重
std::vector<QRect> filterContours(
    const std::vector<std::vector<cv::Point>>& contours,
    int imgW, int imgH)
{
    const int    minSize      = 20;    // 调小以容纳所有常规文本和图标
    const int    maxW         = imgW - 5;
    const int    maxH         = imgH - 5;
    const double maxAspect    = 50.0; // 放宽比例，允许吸附长文本行

    std::vector<QRect> rects;
    rects.reserve(contours.size());

    for (const auto& contour : contours) {
        cv::Rect cvRect = cv::boundingRect(contour);
        int w = cvRect.width;
        int h = cvRect.height;

        if (w < minSize || h < minSize)    continue;
        if (w >= maxW   && h >= maxH)      continue;

        double aspect = static_cast<double>(std::max(w, h)) / std::min(w, h);
        if (aspect > maxAspect) continue;
        
        // 【新增】：修复膨胀腐蚀引起的 1-2 像素偏移
        // 通过稍微内缩 1 个像素，或者至少保证它不越界，让吸附框更贴合实际的 UI 边缘
        int adjustedX = std::max(0, cvRect.x + 1);
        int adjustedY = std::max(0, cvRect.y + 1);
        int adjustedW = std::min(imgW - adjustedX, cvRect.width - 2);
        int adjustedH = std::min(imgH - adjustedY, cvRect.height - 2);

        if (adjustedW >= minSize && adjustedH >= minSize) {
            rects.push_back(QRect(adjustedX, adjustedY, adjustedW, adjustedH));
        }
    }

    // 面积从小到大排序，这样去重时优先保留面积小的（更精细的内部元素）
    std::sort(rects.begin(), rects.end(), [](const QRect& a, const QRect& b) {
        return (a.width() * a.height()) < (b.width() * b.height());
    });

    // 如果数量过多，为了性能截断，只保留面积最大的前一定数量
    // 因为面积非常小的往往是文字或图标细节，通常不需要被吸附
    // 但因为我们已经按面积从小到大排序了，如果要丢弃，应该丢弃最前面的（最小的）
    const size_t MAX_RECTS = 5000;
    if (rects.size() > MAX_RECTS) {
        rects.erase(rects.begin(), rects.begin() + (rects.size() - MAX_RECTS));
    }

    // IoU-NMS：同区域保留面积较小（更精确）的候选框
    // 因为已经按面积从小到大排序，遇到重叠时，直接保留排在前面的（索引更小的）即可
    std::vector<bool> suppressed(rects.size(), false);
    for (size_t i = 0; i < rects.size(); ++i) {
        if (suppressed[i]) continue;
        for (size_t j = i + 1; j < rects.size(); ++j) {
            if (suppressed[j]) continue;
            // 对于包含关系：如果 j 几乎完全包含了 i，或者重叠度极高
            // 我们保留 i (面积小)，suppress j
            if (isHighOverlap(rects[i], rects[j], 0.6)) { // 调低阈值，更容易合并相似框
                suppressed[j] = true;
            }
        }
    }

    std::vector<QRect> result;
    result.reserve(rects.size());
    for (size_t i = 0; i < rects.size(); ++i) {
        if (!suppressed[i]) result.push_back(rects[i]);
    }
    return result;
}

} // namespace

// ==========================================
// OpenCV 智能吸附探测器
// ==========================================

void OpenCVDetector::init(std::shared_ptr<QPixmap> originScreen, std::function<void()> onReady)
{
    if (!originScreen) return;

    // OpenCV 吸附基于图像边缘检测，运行时使用 getRectFromCurrentPoint 的 realTime 分支做窗口命中，
    // 并不读取 HWndRectCacheManager 的缓存；而 cacheAllWindows() 会同步 EnumWindows + 递归
    // EnumChildWindows 枚举所有窗口的全部子控件，在窗口/控件密集系统上耗时 >1s 且对 OpenCV 模式毫无作用，
    // 因此移除它（窗口检测模式仍保留在 SystemWindowDetector::init 中）。
    originScreen_ = originScreen;

    if (onReady) onReady();
}

QRect OpenCVDetector::detectRect(const QPoint& cursorPos, QWidget* window)
{
    if (!originScreen_) return QRect();

    // 1. 获取物理的基础窗口区域
    QRect windowRect;
    if (!window || !Util::getRectFromCurrentPoint(window->winId(), windowRect)) { // PHYSICAL
        return QRect();
    }
    
    QRect physRectInImage = Util::clampToDesktopLocal(Util::toDesktopLocal(windowRect));

    // 将全局逻辑坐标转为窗口内局部坐标
    QPoint localPos = Util::toDesktopLocal(cursorPos);

    // 如果鼠标不在窗口内，直接返回
    if (!physRectInImage.contains(localPos)) {
        return QRect();
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        
        // 2. 检查是否已经计算过该窗口的细粒度矩形
        auto it = cachedWindowRects_.find(physRectInImage);
        if (it != cachedWindowRects_.end()) {
            // 如果计算过，在缓存的精细矩形中寻找最小包围框
            const auto& rects = it->second;
            QRect targetRect = physRectInImage; // 默认兜底为窗口大小
            int minArea = targetRect.width() * targetRect.height();
            
            for (const QRect& r : rects) {
                if (!r.contains(localPos)) continue;
                int area = r.width() * r.height();
                if (area < minArea) {
                    minArea = area;
                    targetRect = r;
                }
            }
            return targetRect;
        }

        // 3. 如果没计算过，且当前没有在处理该窗口，则触发异步计算
        if (processingWindows_.find(physRectInImage) == processingWindows_.end()) {
            processingWindows_.insert(physRectInImage);
            
            // 拷贝一份用于线程内处理
            QRect rectToProcessLogical = physRectInImage;
            QRect rectToProcessPhysical = physRectInImage;
            QImage fullImage = originScreen_->toImage();

            QThread* thread = QThread::create([this, fullImage, rectToProcessLogical, rectToProcessPhysical]() {
                qint64 startTime = QDateTime::currentMSecsSinceEpoch();

                // 裁剪出对应窗口的图像区域进行处理
                QImage windowImage = fullImage.copy(rectToProcessPhysical).convertToFormat(QImage::Format_RGB888);
                
                cv::Mat src(windowImage.height(), windowImage.width(), CV_8UC3,
                            (void*)windowImage.constBits(), windowImage.bytesPerLine());

                cv::Mat bgr;
                cv::cvtColor(src, bgr, cv::COLOR_RGB2BGR);

                cv::Mat gray;
                cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

                cv::Mat edges = detectMultiChannelEdges(bgr, gray);
                edges = morphPostProcess(edges);

                // 在边缘图最外围画一圈白色边框。
                // 作用：让那些横贯或纵贯的 UI 分割线（两端接触边界的开放线条）能够与边框连通，从而闭合成完整的矩形区域。
                cv::rectangle(edges, cv::Point(0, 0), cv::Point(edges.cols - 1, edges.rows - 1), cv::Scalar(255), 1);

                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

                std::vector<QRect> localRects = filterContours(contours, windowImage.width(), windowImage.height());
                
                // 将局部物理坐标转换为全局逻辑坐标
                std::vector<QRect> globalRects;
                globalRects.reserve(localRects.size());
                for (const QRect& r : localRects) {
                    globalRects.push_back(QRect(r.x() + rectToProcessPhysical.x(),
                                                r.y() + rectToProcessPhysical.y(),
                                                r.width(), r.height()));
                }

                qint64 costTime = QDateTime::currentMSecsSinceEpoch() - startTime;
                qDebug() << "OpenCV process window cost time:" << costTime << "ms. Rects count:" << globalRects.size();

                QMetaObject::invokeMethod(qApp, [this, rectToProcessLogical, globalRects]() {
                    std::lock_guard<std::mutex> lock(this->cacheMutex_);
                    this->cachedWindowRects_[rectToProcessLogical] = globalRects;
                    this->processingWindows_.erase(rectToProcessLogical);
                }, Qt::QueuedConnection);
            });

            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            thread->start(QThread::LowPriority);
        }
    }

    // 4. 计算中或者首次触发，直接返回整个窗口区域作为兜底
    return physRectInImage;
}

void OpenCVDetector::reset()
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    originScreen_.reset();
    cachedWindowRects_.clear();
    processingWindows_.clear();
}
