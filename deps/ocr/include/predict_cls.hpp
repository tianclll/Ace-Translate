// include/predict_cls.hpp
#ifndef PREDICT_CLS_HPP
#define PREDICT_CLS_HPP

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <utility>

namespace ocr {

    class TextClassifier {
    public:
        TextClassifier(const std::string& model_path, bool use_gpu = true);
        ~TextClassifier();

        std::pair<std::vector<cv::Mat>, std::vector<std::vector<std::string>>>
        classify(const std::vector<cv::Mat>& images);

    private:
        cv::Mat resizeNormImg(const cv::Mat& img);

        Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "cls"};
        Ort::Session session{nullptr};
        Ort::SessionOptions sessionOptions;
        Ort::MemoryInfo memoryInfo{nullptr};

        std::vector<int> cls_image_shape_ = {3, 48, 192};
        int cls_batch_num_ = 6;
        float cls_thresh_ = 0.9f;
        std::vector<std::string> label_list_ = {"0", "180"};
    };

} // namespace ocr

#endif // PREDICT_CLS_HPP