#include "docmind/utils/ImageUtils.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace docmind {

// ==================== ImageUtils 实现 ====================

    cv::Scalar ImageUtils::getEdgeAverageColor(const cv::Mat& img,
                                               const cv::Rect& rect,
                                               int margin) {
        double b = 0, g = 0, r = 0;
        int count = 0;
        auto addPixel = [&](int x, int y) {
            if (x >= 0 && x < img.cols && y >= 0 && y < img.rows) {
                cv::Vec3b pixel = img.at<cv::Vec3b>(y, x);
                b += pixel[0];
                g += pixel[1];
                r += pixel[2];
                count++;
            }
        };

        int x1 = rect.x, y1 = rect.y, x2 = rect.x + rect.width, y2 = rect.y + rect.height;
        for (int x = x1 - margin; x < x2 + margin; ++x) {
            addPixel(x, y1 - margin);
            addPixel(x, y2 + margin - 1);
        }
        for (int y = y1 - margin + 1; y < y2 + margin - 1; ++y) {
            addPixel(x1 - margin, y);
            addPixel(x2 + margin - 1, y);
        }

        if (count == 0) return cv::Scalar(255, 255, 255);
        return cv::Scalar(b / count, g / count, r / count);
    }

    void ImageUtils::fillFormulaRegions(cv::Mat& roi,
                                        const std::vector<cv::Rect>& rects,
                                        const cv::Mat& original_roi,
                                        int margin) {
        for (const auto& rect : rects) {
            cv::Rect safe_rect = rect & cv::Rect(0, 0, roi.cols, roi.rows);
            if (safe_rect.width <= 0 || safe_rect.height <= 0) continue;
            cv::Scalar color = getEdgeAverageColor(original_roi, rect, margin);
            cv::rectangle(roi, safe_rect, color, cv::FILLED);
        }
    }

    std::string ImageUtils::saveImage(const cv::Mat& image,
                                      const std::string& prefix,
                                      const std::string& assets_dir,
                                      int& counter) {
#ifdef _WIN32
        CreateDirectoryA(assets_dir.c_str(), NULL);
#else
        mkdir(assets_dir.c_str(), 0777);
#endif
        std::string filename = prefix + "_" + std::to_string(++counter) + ".jpg";
        std::string full_path = assets_dir + "/" + filename;
        cv::imwrite(full_path, image);
        return "assets/"+filename;
    }

    bool ImageUtils::overlapY(const cv::Rect& a, const cv::Rect& b, float threshold) {
        int overlap = (std::max)(0, (std::min)(a.y + a.height, b.y + b.height) - (std::max)(a.y, b.y));
        int min_h = (std::min)(a.height, b.height);
        if (min_h == 0) return false;
        return (float)overlap / min_h > threshold;
    }

// ==================== PhotoUtils 实现 ====================

    cv::Rect PhotoUtils::pointsToRect(const std::vector<cv::Point2f>& points) {
        if (points.size() < 4) return cv::Rect();
        return cv::boundingRect(points);
    }

    cv::Mat PhotoUtils::cropImage(const cv::Mat& img, const cv::Rect& rect) {
        if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 ||
            rect.x + rect.width > img.cols || rect.y + rect.height > img.rows) {
            return cv::Mat();
        }
        return img(rect).clone();
    }

    cv::Scalar PhotoUtils::getDominantColor(const cv::Mat& img,
                                            const std::vector<cv::Point2f>& box) {
        cv::Rect rect = pointsToRect(box);
        if (rect.width <= 0 || rect.height <= 0) return cv::Scalar(255, 255, 255);
        cv::Mat roi = img(rect);
        return cv::mean(roi);
    }

    bool PhotoUtils::isInsideImage(const cv::Point2f& pt, const cv::Size& imgSize) {
        return pt.x >= 0 && pt.x < imgSize.width && pt.y >= 0 && pt.y < imgSize.height;
    }

    std::vector<cv::Point2f> PhotoUtils::orderPoints(const std::vector<cv::Point2f>& points) {
        std::vector<cv::Point2f> sorted = points;
        cv::Point2f center(0, 0);
        for (auto& p : sorted) center += p;
        center *= (1.0f / sorted.size());
        std::sort(sorted.begin(), sorted.end(),
                  [&center](const cv::Point2f& a, const cv::Point2f& b) {
                      double angle_a = atan2(a.y - center.y, a.x - center.x);
                      double angle_b = atan2(b.y - center.y, b.x - center.x);
                      return angle_a < angle_b;
                  });
        return sorted;
    }

    cv::Scalar PhotoUtils::getTextColor(const cv::Mat& img,
                                        const std::vector<cv::Point2f>& box) {
        cv::Rect rect = pointsToRect(box);
        if (rect.width < 4 || rect.height < 4) return cv::Scalar(0, 0, 0);
        int cx = rect.x + rect.width / 2;
        int cy = rect.y + rect.height / 2;
        int halfW = static_cast<int>(rect.width * 0.3);
        int halfH = static_cast<int>(rect.height * 0.3);
        cv::Rect center(cx - halfW / 2, cy - halfH / 2, halfW, halfH);
        center = center & cv::Rect(0, 0, img.cols, img.rows);
        if (center.width <= 0 || center.height <= 0) return cv::Scalar(0, 0, 0);
        cv::Mat roi = img(center);
        return cv::mean(roi);
    }

} // namespace docmind