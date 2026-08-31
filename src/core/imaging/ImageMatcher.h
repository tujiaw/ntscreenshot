#pragma once

#include <QImage>
#include <utility>
#include <opencv2/opencv.hpp>

/**
 * @brief Image matching utilities using OpenCV
 *
 * This class provides template matching and image conversion utilities
 * specifically designed for long screenshot stitching.
 */
class ImageMatcher
{
public:
    /**
     * @brief Convert QImage to cv::Mat
     * @param inImage Source QImage
     * @param clone Whether to clone the data (default: false)
     * @return cv::Mat representation of the image
     */
    static cv::Mat QImageToCvMat(const QImage &inImage, bool clone = false);

    /**
     * @brief Estimate scroll shift between two consecutive screenshots
     *
     * This function uses template matching to find how much the second image
     * has scrolled relative to the first image. It's optimized for screen
     * captures where content is identical but position has shifted.
     *
     * @param previous Previous frame
     * @param current Current frame
     * @param minShift Minimum shift to consider (in pixels)
     * @param maxShift Maximum shift to consider (in pixels)
     * @return Pair of (shift in pixels, match score where 0 = perfect match)
     */
    static std::pair<int, double> estimateScrollShift(
        const QImage& previous,
        const QImage& current,
        int minShift,
        int maxShift
    );

    /**
     * @brief Constants for matching configuration
     */
    static constexpr int MATCH_MARGIN_H_PERCENT = 15;  // Ignore 15% margin (scrollbars)
    static constexpr int DEFAULT_TEMPLATE_HEIGHT = 100; // Reduced from 200 for better performance
    static constexpr int MIN_TEMPLATE_HEIGHT = 40;      // Reduced from 50
    static constexpr int MAX_TEMPLATE_HEIGHT_RATIO = 6; // Template height is at most image height / 6
};
