#ifndef OCR_HPP
#define OCR_HPP

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include "export.hpp"
#include "types.hpp"

namespace ocr {

    // ============================================================
    // Ocr - 完整 OCR 识别（检测 + 识别）
    // ============================================================
    class OCR_API Ocr {
    public:
        /**
         * 初始化 OCR
         * @param model_dir 模型目录，默认 "./models"
         * @param use_gpu   是否使用 GPU
         */
        explicit Ocr(const std::string& model_dir = "", bool use_gpu = false);
        ~Ocr();

        /**
         * 识别图像 (cv::Mat)
         */
        std::vector<OCRResult> recognize(const cv::Mat& img);

        /**
         * 识别图像 (文件路径)
         */
        std::vector<OCRResult> recognize(const std::string& image_path);
        // 新增：只识别不检测（单张裁剪图像）
        std::vector<OCRResult> Ocr::recognizeCrop(const cv::Mat& img);

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    // ============================================================
    // OCRDetector - 独立的文本检测器（只检测，不识别）
    // ============================================================
    class OCR_API OCRDetector {
    public:
        /**
         * 初始化检测器
         * @param model_path 检测模型路径（ONNX）
         * @param use_gpu    是否使用 GPU
         */
        explicit OCRDetector(const std::string& model_path, bool use_gpu = false);
        ~OCRDetector();

        /**
         * 执行文本检测
         * @param img OpenCV 图像 (BGR)
         * @return 检测框列表，每个框为4个点 (cv::Point2f)
         */
        std::vector<std::vector<cv::Point2f>> detect(const cv::Mat& img);

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    // ============================================================
    // C 接口（用于跨语言调用）
    // ============================================================
#ifdef __cplusplus
    extern "C" {
#endif
// include/ocr.hpp - extern "C" 块中


    // ---------- OCR 完整识别 ----------
    OCR_API void* ocr_create(const char* model_dir, int use_gpu);
    OCR_API void  ocr_destroy(void* ocr);
    OCR_API char* ocr_recognize_file(void* ocr, const char* image_path);
    OCR_API char* ocr_recognize_buffer(void* ocr, const unsigned char* data, int width, int height, int channels);
    OCR_API char* ocr_recognize_crop(void* ocr, const unsigned char* data, int width, int height, int channels);
    // ---------- 检测器 ----------
    OCR_API void* ocr_detector_create(const char* model_path, int use_gpu);
    OCR_API void  ocr_detector_destroy(void* detector);
    OCR_API char* ocr_detector_detect_file(void* detector, const char* image_path);
    OCR_API char* ocr_detector_detect_buffer(void* detector, const unsigned char* data, int width, int height, int channels);

    // ---------- 通用 ----------
    OCR_API void ocr_free_string(char* str);

#ifdef __cplusplus
    }
#endif

} // namespace ocr

#endif // OCR_HPP