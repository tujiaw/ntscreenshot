#include "ImageMatcher.h"
#include "CvBridge.h"
#include <opencv2/imgproc.hpp>
#include <limits>
#include <QDebug>

// Validate a candidate against the actual overlapping pixels. Feature votes and
// correlation peaks alone are ambiguous on repeated rows and fixed page chrome.
static double overlapError(const cv::Mat& previous, const cv::Mat& current, int shift)
{
    if (shift < 0 || shift >= previous.rows || previous.size() != current.size()) {
        return 1.0;
    }
    const int x0 = previous.cols * 15 / 100;
    const int x1 = previous.cols * 85 / 100;
    const int y0 = qMax(0, previous.rows * 10 / 100);
    const int y1 = qMin(previous.rows - shift, previous.rows * 90 / 100);
    if (x1 <= x0 || y1 - y0 < 20) {
        return 1.0;
    }

    double difference = 0.0;
    int samples = 0;
    for (int y = y0; y < y1; y += 3) {
        const uchar* oldLine = previous.ptr<uchar>(y + shift);
        const uchar* newLine = current.ptr<uchar>(y);
        for (int x = x0; x < x1; x += 3) {
            // Blank background should not dilute a mismatch between text rows.
            if (oldLine[x] >= 245 && newLine[x] >= 245) continue;
            difference += std::abs(int(oldLine[x]) - int(newLine[x]));
            ++samples;
        }
    }
    return samples ? difference / (samples * 255.0) : 1.0;
}

static std::pair<int, double> validatedShift(
    const cv::Mat& previous, const cv::Mat& current, int shift)
{
    const double error = overlapError(previous, current, shift);
    const double stationaryError = overlapError(previous, current, 0);
    if (error >= 0.15 || (shift > 0 && error >= stationaryError * 0.9)) {
        return {-1, std::numeric_limits<double>::max()};
    }
    return {shift, error};
}

// 相位相关法：在频域中计算两帧之间的全局位移。
// 该方法对周期性重复内容（如表格行）天然免疫，因为它寻找的是整体图像中
// 最主导的平移量，而非依赖局部块的空间唯一性。
//
// phaseCorrelate 返回 (dx, dy)：将 src2 对齐到 src1 所需的偏移量。
// 当内容向上滚动 s 像素时：curr(y) = prev(y + s)
// 即 curr 是 prev 在 y 方向平移了 -s，所以 phaseCorrelate(prev, curr) 返回 dy = -s
// 因此需对 dy 取反以得到我们定义的正向 shift（向下滚动为正）。
static std::pair<int, double> tryPhaseCorrelation(
    const cv::Mat& gray1,
    const cv::Mat& gray2,
    int minShift,
    int maxShift)
{
    if (gray1.size() != gray2.size() || gray1.empty()) {
        return {-1, std::numeric_limits<double>::max()};
    }

    cv::Mat f1, f2;
    gray1.convertTo(f1, CV_64F);
    gray2.convertTo(f2, CV_64F);

    cv::Mat hann;
    cv::createHanningWindow(hann, f1.size(), CV_64F);

    double response = 0.0;
    cv::Point2d shiftPt = cv::phaseCorrelate(f1, f2, hann, &response);

    // 相位相关返回 dy = -scroll_shift，取反得到正向滚动量
    int shift = -static_cast<int>(std::round(shiftPt.y));

    qDebug() << "[ImageMatcher] Phase correlation raw: shift=" << shift << "response=" << response;

    // response 越高表示相关峰越明显。0.1 是宽松阈值，足以排除纯随机噪声；
    // 对有内容的表格（存在不同产品名、编号等行）通常可达 0.15 以上。
    if (shift < minShift || shift > maxShift || response < 0.1) {
        return {-1, std::numeric_limits<double>::max()};
    }

    return {shift, 0.0};
}

cv::Mat ImageMatcher::QImageToCvMat(const QImage &inImage, bool clone)
{
    return CvBridge::QImageToMat(inImage, clone);
}

std::pair<int, double> ImageMatcher::estimateScrollShift(
    const QImage& previous,
    const QImage& current,
    int minShift,
    int maxShift)
{
    // 将输入的 QImage 转换为 OpenCV 的 Mat 格式，以便进行图像处理
    cv::Mat prevMat = QImageToCvMat(previous);
    cv::Mat currMat = QImageToCvMat(current);

    // 检查图像是否转换成功或是否为空
    if (prevMat.empty() || currMat.empty() || prevMat.size() != currMat.size()) {
        qDebug() << "[ImageMatcher] Error: Empty Mat.";
        return {-1, std::numeric_limits<double>::max()};
    }

    qDebug() << "[ImageMatcher] estimateScrollShift start, img height:" << previous.height()
             << "minShift:" << minShift << "maxShift:" << maxShift;

    // 将彩色图像转换为灰度图像，大幅提升模板匹配的计算速度
    cv::Mat grayPrev, grayCurr;
    cv::cvtColor(prevMat, grayPrev, prevMat.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_RGB2GRAY);
    cv::cvtColor(currMat, grayCurr, currMat.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_RGB2GRAY);

    // 确定模板区域 (ROI - Region of Interest)
    // 方案五混合方案：如果 ORB 特征点太少（可能是大片空白或纯色导致），自动降级为模板匹配
    bool useTemplateFallback = false;
    int templateHeight = qMin(DEFAULT_TEMPLATE_HEIGHT, previous.height() / MAX_TEMPLATE_HEIGHT_RATIO);
    if (templateHeight < MIN_TEMPLATE_HEIGHT) templateHeight = MIN_TEMPLATE_HEIGHT;

    // 安全检查：确保图像的高度大于模板的高度
    if (previous.height() < templateHeight || current.height() < templateHeight) {
        return {-1, std::numeric_limits<double>::max()};
    }

    // --- 优先尝试：基于特征点 (ORB + RANSAC) 的精确匹配 ---
    // 这个方案能完全忽略局部的动画、视频、悬停变化
    cv::Ptr<cv::ORB> orb = cv::ORB::create(500);

    // 截取当前帧和上一帧底部的搜索区域以提高性能
    int searchHeight = qMin(previous.height(), current.height());
    int cropHeight = qMin(searchHeight, qMax(300, searchHeight / 2)); // 调低最小截取高度，适应较小的截图区域
    
    int prevCropY = previous.height() - cropHeight;
    int currCropY = current.height() - cropHeight;

    // 重新引入 marginX 裁剪，使得 ORB 专注于核心滚动区域，
    // 避免被两侧不滚动的导航栏/滚动条的特征点（shift=0）占据多数票
    int marginX = previous.width() * MATCH_MARGIN_H_PERCENT / 100;
    int searchWidth = previous.width() - 2 * marginX;
    if (searchWidth <= 0) return {-1, std::numeric_limits<double>::max()};

    cv::Rect roiPrev(marginX, prevCropY, searchWidth, cropHeight);
    cv::Rect roiCurr(marginX, currCropY, searchWidth, cropHeight);

    cv::Mat prevSub = grayPrev(roiPrev);
    cv::Mat currSub = grayCurr(roiCurr);

    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    cv::Mat descriptors1, descriptors2;
    orb->detectAndCompute(prevSub, cv::noArray(), keypoints1, descriptors1);
    orb->detectAndCompute(currSub, cv::noArray(), keypoints2, descriptors2);

    qDebug() << "[ImageMatcher] ORB keypoints - prev:" << keypoints1.size() << "curr:" << keypoints2.size();

    // 关键点：大片空白/纯色区域找不出特征点，或者特征点太少，此时必须降级
    if (keypoints1.size() < 10 || keypoints2.size() < 10 || descriptors1.empty() || descriptors2.empty()) {
        qDebug() << "[ImageMatcher] Too few keypoints, fallback to template matching.";
        useTemplateFallback = true;
    } else {
        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<cv::DMatch> matches;
        matcher.match(descriptors1, descriptors2, matches);

        std::vector<int> yShifts;
        for (const auto& match : matches) {
            cv::Point2f pt1 = keypoints1[match.queryIdx].pt;
            cv::Point2f pt2 = keypoints2[match.trainIdx].pt;

            // 允许 X 轴方向有微小的容差
            if (std::abs(pt1.x - pt2.x) < 2.0f) {
                int shift = std::round((pt1.y + prevCropY) - (pt2.y + currCropY));
                // 注意：由于外部传入的 minShift 是 -10，这里允许 shift=0 的点进入投票。
                // 这非常关键！如果实际没滚动（仅有局部 hover 导致重绘），我们要算出真实的 0，在外层将其拦截。
                if (shift >= minShift && shift <= maxShift) {
                    yShifts.push_back(shift);
                }
            }
        }

        if (yShifts.empty()) {
            qDebug() << "[ImageMatcher] No valid ORB shifts found, fallback.";
            useTemplateFallback = true;
        } else {
            std::sort(yShifts.begin(), yShifts.end());
            
            int bestShift = -1;
            int maxCount = 0;
            
            for (size_t i = 0; i < yShifts.size(); ++i) {
                int currentCount = 1;
                int currentSum = yShifts[i];
                
                for (size_t j = i + 1; j < yShifts.size(); ++j) {
                    if (std::abs(yShifts[j] - yShifts[i]) <= 1) {
                        currentCount++;
                        currentSum += yShifts[j];
                    } else {
                        break;
                    }
                }
                
                if (currentCount > maxCount) {
                    maxCount = currentCount;
                    bestShift = std::round(static_cast<double>(currentSum) / currentCount);
                }
            }

            // 如果支持该 shift 的特征点数量太少（认为不可靠），也降级
            if (maxCount < 5) {
                qDebug() << "[ImageMatcher] ORB maxCount too low:" << maxCount << "fallback.";
                useTemplateFallback = true;
            } else if (bestShift <= 0) {
                // ORB 以高票数报告 shift=0，但这可能是表格重复列（如固定的"中国/墨西哥/徐州"）
                // 造成的虚假共识：不同行的相同内容被错误匹配到同一位置。
                // 转交给相位相关法做频域验证，它不受空间重复性干扰。
                qDebug() << "[ImageMatcher] ORB shift<=0 (maxCount:" << maxCount << "), may be table content trap, trying phase correlation.";
                useTemplateFallback = true;
            } else {
                auto validated = validatedShift(grayPrev, grayCurr, bestShift);
                if (validated.first >= 0) {
                    return validated;
                }
                qDebug() << "[ImageMatcher] ORB overlap validation failed:" << bestShift;
                useTemplateFallback = true;
            }
        }
    }

    // --- 降级方案一：相位相关法 (Phase Correlation Fallback) ---
    // 专门应对周期性重复内容，如表格行、代码列表等。
    // 相位相关在频域工作，对空间上的重复结构天然免疫：
    // 无论有多少相似的行，它只寻找使整体图像对齐的那一个全局平移量。
    // 不使用水平边距裁剪，因为 Hanning 窗口已处理了边缘效应，
    // 且保留表格边缘列（序号、编号）有助于提升相关峰的清晰度。
    if (useTemplateFallback) {
        auto [pcShift, pcScore] = tryPhaseCorrelation(grayPrev, grayCurr, minShift, maxShift);
        if (pcShift >= minShift && pcShift <= maxShift && pcScore < std::numeric_limits<double>::max()) {
            qDebug() << "[ImageMatcher] Phase correlation matched: shift=" << pcShift;
            auto validated = validatedShift(grayPrev, grayCurr, pcShift);
            if (validated.first >= 0) return validated;
        }
        qDebug() << "[ImageMatcher] Phase correlation insufficient, falling back to template matching.";
    }

    // --- 降级方案二：多候选模板匹配 (Template Match Fallback) ---
    // 专门应对大片空白或纯色区域（如记事本末尾、网页留白处）
    // 同时在底部 / 中下部 / 中部取多个模板候选，选置信度最高且位移合法者，减少表格误拼。
    if (useTemplateFallback) {
        qDebug() << "[ImageMatcher] Entering Template Match Fallback.";
        int marginX = previous.width() * MATCH_MARGIN_H_PERCENT / 100;
        int searchWidth = previous.width() - 2 * marginX;
        if (searchWidth <= 0) return {-1, std::numeric_limits<double>::max()};

        const int candidates[] = {
            previous.height() - templateHeight,
            previous.height() * 2 / 3 - templateHeight / 2,
            previous.height() / 2 - templateHeight / 2,
        };

        int bestShift = -1;
        double bestMaxVal = -1.0;

        for (int candidateBase : candidates) {
            int baseY = candidateBase;
            if (baseY < 0) {
                baseY = 0;
            }
            if (baseY + templateHeight > previous.height()) {
                continue;
            }

            cv::Rect templateRoi(marginX, baseY, searchWidth, templateHeight);
            cv::Mat templ = grayPrev(templateRoi);

            cv::Mat mean, stddev;
            cv::meanStdDev(templ, mean, stddev);
            while (stddev.at<double>(0) < 3.0 && baseY > 0) {
                baseY -= 10;
                if (baseY < 0) baseY = 0;
                templateRoi = cv::Rect(marginX, baseY, searchWidth, templateHeight);
                templ = grayPrev(templateRoi);
                cv::meanStdDev(templ, mean, stddev);
            }

            int searchYStart = baseY - maxShift;
            int searchYEnd = baseY - minShift + templateHeight;
            if (searchYStart < 0) searchYStart = 0;
            if (searchYEnd > current.height()) searchYEnd = current.height();
            if (searchYEnd - searchYStart < templateHeight) {
                continue;
            }

            cv::Rect searchRoi(marginX, searchYStart, searchWidth, searchYEnd - searchYStart);
            cv::Mat searchImg = grayCurr(searchRoi);

            cv::Mat result;
            cv::matchTemplate(searchImg, templ, result, cv::TM_CCOEFF_NORMED);

            double minVal = 0.0;
            double maxVal = 0.0;
            cv::Point minLoc;
            cv::Point maxLoc;
            cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

            const int currentMatchY = searchYStart + maxLoc.y;
            const int shift = baseY - currentMatchY;
            if (shift < minShift || shift > maxShift) {
                continue;
            }
            if (maxVal > bestMaxVal) {
                bestMaxVal = maxVal;
                bestShift = shift;
            }
        }

        if (bestShift < 0) {
            return {-1, std::numeric_limits<double>::max()};
        }

        auto validated = bestMaxVal >= 0.70
            ? validatedShift(grayPrev, grayCurr, bestShift)
            : std::pair<int, double>{-1, std::numeric_limits<double>::max()};
        qDebug() << "[ImageMatcher] Multi-template match done, shift:" << bestShift
                 << "maxVal:" << bestMaxVal << "score:" << validated.second;
        return validated;
    }

    qDebug() << "[ImageMatcher] Fallback failed.";
    return {-1, std::numeric_limits<double>::max()};
}
