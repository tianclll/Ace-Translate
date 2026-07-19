// include/predict_system.hpp
#ifndef PREDICT_SYSTEM_HPP
#define PREDICT_SYSTEM_HPP

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "export.hpp"
#include "types.hpp"

namespace ocr {

    class OCR_API TextSystem {
    public:
        TextSystem(const std::string& det_model_path,
                   const std::string& rec_model_path,
                   const std::string& rec_dict_path,
                   const std::string& cls_model_path,
                   bool use_gpu = false);

        ~TextSystem();

        std::vector<OCRResult> recognize(const cv::Mat& img);
        // 新增：只识别不检测（直接对裁剪区域识别）
        std::vector<OCRResult> recognizeCrop(const cv::Mat& img);
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace ocr

#endif // PREDICT_SYSTEM_HPP