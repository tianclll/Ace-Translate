#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace docmind {

// 版面检测结果框
    struct BoxInfo {
        cv::Rect bbox;
        int class_id;
        std::string label;
        float score;
    };

// OCR 识别结果
    struct OCRResult {
        std::vector<std::vector<float>> box;  // 四点坐标 [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]
        std::string text;
        float score;
    };

// 内部元素（用于文本行组合）
    struct Element {
        cv::Rect bbox;
        std::string text;
        bool is_formula;
        bool used;
    };

} // namespace docmind