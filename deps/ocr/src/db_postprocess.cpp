// src/db_postprocess.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "db_postprocess.hpp"
#include <clipper2/clipper.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ocr {

    using namespace Clipper2Lib;

    DBPostProcess::DBPostProcess(
            float thresh,
            float box_thresh,
            float unclip_ratio,
            int max_candidates)
            : thresh_(thresh),
              box_thresh_(box_thresh),
              unclip_ratio_(unclip_ratio),
              max_candidates_(max_candidates) {
    }

    float DBPostProcess::boxScoreFast(
            const cv::Mat& bitmap,
            const std::vector<cv::Point2f>& box) {

        int h = bitmap.rows;
        int w = bitmap.cols;

        float xmin = w;
        float xmax = 0;
        float ymin = h;
        float ymax = 0;

        for (auto& p : box) {
            xmin = (std::min)(xmin, p.x);
            xmax = (std::max)(xmax, p.x);
            ymin = (std::min)(ymin, p.y);
            ymax = (std::max)(ymax, p.y);
        }

        int x0 = (std::max)(0, (int)std::floor(xmin));
        int x1 = (std::min)(w - 1, (int)std::ceil(xmax));
        int y0 = (std::max)(0, (int)std::floor(ymin));
        int y1 = (std::min)(h - 1, (int)std::ceil(ymax));

        cv::Mat mask = cv::Mat::zeros(y1 - y0 + 1, x1 - x0 + 1, CV_8UC1);

        std::vector<cv::Point> pts;
        for (auto& p : box) {
            pts.emplace_back((int)(p.x - x0), (int)(p.y - y0));
        }

        cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{pts}, 1);

        return (float)cv::mean(bitmap(cv::Range(y0, y1 + 1), cv::Range(x0, x1 + 1)), mask)[0];
    }

    std::vector<cv::Point2f> DBPostProcess::unclip(
            const std::vector<cv::Point2f>& box,
            float ratio) {

        std::vector<cv::Point> contour;
        for (auto& p : box) {
            contour.emplace_back((int)p.x, (int)p.y);
        }

        double area = cv::contourArea(contour);
        double length = cv::arcLength(contour, true);

        if (length < 1e-6) return {};

        double distance = area * ratio / length;

        PathD path;
        for (auto& p : box) {
            path.push_back(PointD(p.x, p.y));
        }

        PathsD solution = InflatePaths({path}, distance, JoinType::Round, EndType::Polygon);

        if (solution.empty() || solution.size() > 1) return {};

        std::vector<cv::Point2f> result;
        for (auto& p : solution[0]) {
            result.emplace_back((float)p.x, (float)p.y);
        }

        return result;
    }

    std::pair<std::vector<cv::Point2f>, float> DBPostProcess::getMiniBoxes(
            const std::vector<cv::Point2f>& contour) {

        cv::RotatedRect rect = cv::minAreaRect(contour);
        cv::Point2f pts[4];
        rect.points(pts);

        std::vector<cv::Point2f> box(pts, pts + 4);

        std::sort(box.begin(), box.end(), [](auto& a, auto& b) {
            return a.x < b.x;
        });

        int index1, index2, index3, index4;
        if (box[1].y > box[0].y) {
            index1 = 0;
            index4 = 1;
        } else {
            index1 = 1;
            index4 = 0;
        }

        if (box[3].y > box[2].y) {
            index2 = 2;
            index3 = 3;
        } else {
            index2 = 3;
            index3 = 2;
        }

        std::vector<cv::Point2f> result = {
                box[index1],
                box[index2],
                box[index3],
                box[index4]
        };

        return {result, (std::min)(rect.size.width, rect.size.height)};
    }

    std::vector<std::vector<cv::Point2f>> DBPostProcess::process(
            const cv::Mat& pred,
            int src_w,
            int src_h,
            float scale_x,
            float scale_y) {


        // 二值化
        cv::Mat segmentation;
        cv::threshold(pred, segmentation, thresh_, 255, cv::THRESH_BINARY);
        segmentation.convertTo(segmentation, CV_8UC1);

        // 查找轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(segmentation, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);


        int num_contours = (std::min)((int)contours.size(), max_candidates_);

        std::vector<std::vector<cv::Point2f>> boxes;

        for (int i = 0; i < num_contours; ++i) {
            // 多边形近似 (与Python完全一致)
            std::vector<cv::Point> approx;
            double perimeter = cv::arcLength(contours[i], true);
            double epsilon = 0.002 * perimeter;
            cv::approxPolyDP(contours[i], approx, epsilon, true);

            if (approx.size() < 4) continue;

            std::vector<cv::Point2f> contour_pts;
            for (auto& p : approx) {
                contour_pts.emplace_back((float)p.x, (float)p.y);
            }

            auto mini = getMiniBoxes(contour_pts);
            auto points = mini.first;
            float sside = mini.second;

            if (sside < min_size_) continue;

            float score = boxScoreFast(pred, points);
            if (score < box_thresh_) continue;

            auto expanded = unclip(points, unclip_ratio_);
            if (expanded.empty()) continue;

            auto mini2 = getMiniBoxes(expanded);
            auto box = mini2.first;
            sside = mini2.second;

            if (sside < min_size_ + 2) continue;

            // 缩放到原图
            for (auto& p : box) {
                p.x *= scale_x;
                p.y *= scale_y;
                p.x = (std::clamp)(p.x, 0.f, (float)src_w);
                p.y = (std::clamp)(p.y, 0.f, (float)src_h);
            }

            boxes.push_back(box);
        }

        return boxes;
    }

} // namespace ocr