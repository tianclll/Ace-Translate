#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace docmind {

// ==================== 文档图片处理工具 ====================
    class ImageUtils {
    public:
        static cv::Scalar getEdgeAverageColor(const cv::Mat& img,
                                              const cv::Rect& rect,
                                              int margin = 3);
        static void fillFormulaRegions(cv::Mat& roi,
                                       const std::vector<cv::Rect>& rects,
                                       const cv::Mat& original_roi,
                                       int margin = 3);
        static std::string saveImage(const cv::Mat& image,
                                     const std::string& prefix,
                                     const std::string& assets_dir,
                                     int& counter);
        static bool overlapY(const cv::Rect& a,
                             const cv::Rect& b,
                             float threshold = 0.6f);
    };

// ==================== 通用图像工具 ====================
    class PhotoUtils {
    public:
        static cv::Rect pointsToRect(const std::vector<cv::Point2f>& points);
        static cv::Mat cropImage(const cv::Mat& img, const cv::Rect& rect);
        static cv::Scalar getDominantColor(const cv::Mat& img,
                                           const std::vector<cv::Point2f>& box);
        static bool isInsideImage(const cv::Point2f& pt, const cv::Size& imgSize);
        static std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point2f>& points);
        static cv::Scalar getTextColor(const cv::Mat& img,
                                       const std::vector<cv::Point2f>& box);
    };

} // namespace docmind