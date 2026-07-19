// include/predict_rec.hpp
#ifndef PREDICT_REC_HPP
#define PREDICT_REC_HPP

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <utility>
#include <memory>

namespace ocr {

    class TextRecognizer {
    public:
        // 构造函数增加 max_text_length 参数，默认 50
        TextRecognizer(const std::string& model_path,
                       const std::string& dict_path,
                       bool use_gpu = false,
                       int max_text_length = 50);
        ~TextRecognizer();

        std::vector<std::pair<std::string, float>> recognize(const std::vector<cv::Mat>& images);

    private:
        cv::Mat preprocess(const cv::Mat& img, float max_wh_ratio);
        std::vector<float> normalize(const cv::Mat& img, int imgW);
        void loadDict(const std::string& dict_path);
        // CTC解码，增加最大长度参数
        std::string ctcDecode(const std::vector<int>& pred_indices, int max_length);

        Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "recognition"};
        Ort::Session session{nullptr};
        Ort::SessionOptions sessionOptions;
        Ort::MemoryInfo memoryInfo{nullptr};
        std::vector<int64_t> inputShape;
        std::vector<std::string> char_list_;
        int batch_size_ = 6;
        int imgH_ = 48;
        int imgC_ = 3;
        float drop_score_ = 0.5f;
        int max_text_length_ = 10000;  // 新增成员
    };

} // namespace ocr

#endif // PREDICT_REC_HPP