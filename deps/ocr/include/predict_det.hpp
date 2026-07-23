// include/predict_det.hpp
#ifndef PREDICT_DET_HPP
#define PREDICT_DET_HPP

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

namespace ocr {

    class TextDetector {
    public:
        TextDetector(const std::string& model_path, bool use_gpu = true);
        ~TextDetector();

        std::vector<std::vector<cv::Point2f>> detect(const cv::Mat& img);

    private:
        cv::Mat preprocess(const cv::Mat& img, float& scale_x, float& scale_y, int& new_w, int& new_h);

        Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "detection"};
        Ort::Session session{nullptr};
        Ort::SessionOptions sessionOptions;
        Ort::MemoryInfo memoryInfo{nullptr};
        std::vector<int64_t> inputShape;

        // ========== 与 Python PPOCRv6 tiny 完全对齐 ==========
        int input_size_ = 736;              // det_limit_side_len
        float db_thresh_ = 0.2f;            // det_db_thresh
        float box_thresh_ = 0.4f;           // det_db_box_thresh (降低以检测彩色/弱对比文字)
        float unclip_ratio_ = 1.4f;         // det_db_unclip_ratio
        int max_candidates_ = 3000;         // det_db_max_candidates
        int min_size_ = 3;                  // 最小尺寸
    };

} // namespace ocr

#endif // PREDICT_DET_HPP