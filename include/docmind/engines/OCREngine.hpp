#pragma once

#include "docmind/types.hpp"
#include "DLLLoader.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace docmind {

    class OCREngine {
    public:
        OCREngine(DLLLoader& loader, const std::string& model_dir, bool use_gpu = true);
        ~OCREngine();

        // 识别整个图像（缓冲区）
        std::vector<OCRResult> recognizeBuffer(const cv::Mat& image);
        // 识别裁剪区域（自动选择合适函数）
        OCRResult recognizeCrop(const cv::Mat& crop);
        // 解析OCR JSON 字符串
        static std::vector<OCRResult> parseResults(const std::string& json);

    private:
        DLLLoader& dll_;
        void* handle_ = nullptr;
        bool initialized_ = false;
    };

} // namespace docmind