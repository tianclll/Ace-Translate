#pragma once

#include <string>
#include <opencv2/opencv.hpp>

namespace docmind {

    class PhotoProcessor {
    public:
        // 预留，暂不实现
        std::string process(const cv::Mat& image, const std::string& target_language);
    };

} // namespace docmind