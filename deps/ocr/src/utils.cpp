// src/utils.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ocr {

    cv::Mat getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box) {
        if (box.size() != 4) {
            return cv::Mat();
        }

        // 计算目标宽度和高度
        float width1 = std::sqrt(std::pow(box[0].x - box[1].x, 2) + std::pow(box[0].y - box[1].y, 2));
        float width2 = std::sqrt(std::pow(box[2].x - box[3].x, 2) + std::pow(box[2].y - box[3].y, 2));
        float height1 = std::sqrt(std::pow(box[0].x - box[3].x, 2) + std::pow(box[0].y - box[3].y, 2));
        float height2 = std::sqrt(std::pow(box[1].x - box[2].x, 2) + std::pow(box[1].y - box[2].y, 2));

        int crop_width = static_cast<int>((std::max)(width1, width2));
        int crop_height = static_cast<int>((std::max)(height1, height2));

        // 透视变换
        cv::Point2f src_pts[4] = {box[0], box[1], box[2], box[3]};
        cv::Point2f dst_pts[4] = {
                cv::Point2f(0, 0),
                cv::Point2f(crop_width - 1, 0),
                cv::Point2f(crop_width - 1, crop_height - 1),
                cv::Point2f(0, crop_height - 1)
        };

        cv::Mat M = cv::getPerspectiveTransform(src_pts, dst_pts);
        cv::Mat dst;
        cv::warpPerspective(img, dst, M, cv::Size(crop_width, crop_height),
                            cv::INTER_CUBIC, cv::BORDER_REPLICATE);

        return dst;
    }

    std::vector<cv::Point2f> orderPointsClockwise(const std::vector<cv::Point2f>& pts) {
        std::vector<cv::Point2f> rect(4);

        // 计算中心点
        float cx = 0, cy = 0;
        for (const auto& pt : pts) {
            cx += pt.x;
            cy += pt.y;
        }
        cx /= pts.size();
        cy /= pts.size();

        // 按角度排序
        std::vector<int> indices(pts.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
                  [&](int i, int j) {
                      float angle_i = std::atan2(pts[i].y - cy, pts[i].x - cx);
                      float angle_j = std::atan2(pts[j].y - cy, pts[j].x - cx);
                      return angle_i < angle_j;
                  });

        for (size_t i = 0; i < 4 && i < indices.size(); ++i) {
            rect[i] = pts[indices[i]];
        }

        return rect;
    }

    void sortBoxes(std::vector<std::vector<cv::Point2f>>& boxes) {
        std::sort(boxes.begin(), boxes.end(),
                  [](const std::vector<cv::Point2f>& a, const std::vector<cv::Point2f>& b) {
                      if (a.empty() || b.empty()) return false;
                      return a[0].y < b[0].y;
                  });
    }

} // namespace ocr