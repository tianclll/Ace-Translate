// include/utils.hpp
#ifndef UTILS_HPP
#define UTILS_HPP

#include <opencv2/opencv.hpp>
#include <vector>

namespace ocr {

    cv::Mat getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box);

    std::vector<cv::Point2f> orderPointsClockwise(const std::vector<cv::Point2f>& pts);

    void sortBoxes(std::vector<std::vector<cv::Point2f>>& boxes);

} // namespace ocr

#endif // UTILS_HPP