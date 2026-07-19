// src/predict_system.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "predict_system.hpp"
#include "predict_det.hpp"
#include "predict_rec.hpp"
#include "predict_cls.hpp"
#include "utils.hpp"
#include <algorithm>
#include <memory>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <opencv2/opencv.hpp>

namespace ocr {

    class TextSystem::Impl {
    public:
        Impl(const std::string& det_model_path,
             const std::string& rec_model_path,
             const std::string& rec_dict_path,
             const std::string& cls_model_path,
             bool use_gpu)
                : use_gpu_(use_gpu) {

            std::cout << "Loading detection model..." << std::endl;
            detector_ = std::make_unique<TextDetector>(det_model_path, use_gpu);

            std::cout << "Loading recognition model..." << std::endl;
            recognizer_ = std::make_unique<TextRecognizer>(rec_model_path, rec_dict_path, use_gpu);

            std::cout << "Loading cls model..." << std::endl;
            classifier_ = std::make_unique<TextClassifier>(cls_model_path, use_gpu);

            drop_score_ = 0.3f;
            std::cout << "All models loaded!" << std::endl;

            save_crops_ = true;
            crop_counter_ = 0;
        }

        std::vector<OCRResult> recognize(const cv::Mat& img) {
            std::vector<OCRResult> results;
            auto start = std::chrono::high_resolution_clock::now();
            // 1. 文本检测
            auto boxes = detector_->detect(img);

            if (boxes.empty()) {
                return results;
            }

            // 2. 排序
            sortBoxes(boxes);

            // 3. 裁剪
            std::vector<cv::Mat> crop_images;
            std::vector<size_t> valid_box_indices;

            crop_counter_ = 0;
            for (size_t i = 0; i < boxes.size(); ++i) {
                cv::Mat crop = getRotateCropImage(img, boxes[i]);

                if (!crop.empty() && crop.cols >= 10 && crop.rows >= 10) {

                    crop_images.push_back(crop);
                    valid_box_indices.push_back(i);
                }
            }


            if (crop_images.empty()) {
                return results;
            }

            // ============================================================
            // 4. 方向分类 - 获取旋转后的图像和分类结果
            // ============================================================



            // 在 recognize 函数中
//            auto cls_result = classifier_->classify(crop_images);
//            auto oriented_images = cls_result.first;  // 旋转后的图像
//            auto cls_results = cls_result.second;      // 分类结果
            auto oriented_images = crop_images;


            // ============================================================
            // 5. 文本识别 - 传入旋转后的图像 (vector<Mat>)
            // ============================================================
            std::cout << "Text recognition..." << std::endl;
            auto rec_results = recognizer_->recognize(oriented_images);

            // ============================================================
            // 6. 过滤低置信度结果
            // ============================================================
            for (size_t i = 0; i < rec_results.size(); ++i) {
                size_t box_idx = valid_box_indices[i];

                if (rec_results[i].second >= drop_score_ && !rec_results[i].first.empty()) {
                    OCRResult result;
                    for (const auto& pt : boxes[box_idx]) {
                        result.box.push_back({pt.x, pt.y});
                    }
                    result.text = rec_results[i].first;
                    result.score = rec_results[i].second;
                    results.push_back(result);
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);


            return results;
        }
// 新增：只识别不检测
        std::vector<OCRResult> recognizeCrop(const cv::Mat& img) {
            OCRResult result;
            std::vector<OCRResult> results;
            if (img.empty()) {
                return results;
            }

            // 直接调用识别器，跳过检测
            std::vector<cv::Mat> crops = {img};
            auto rec_results = recognizer_->recognize(crops);

            if (!rec_results.empty() && rec_results[0].second >= drop_score_ &&
                !rec_results[0].first.empty()) {
                result.text = rec_results[0].first;
                result.score = rec_results[0].second;
                // box 保持空（无需坐标）
            }
            results.push_back(result);
            return results;
        }

        static void sortBoxes(std::vector<std::vector<cv::Point2f>>& boxes) {
            std::sort(boxes.begin(), boxes.end(),
                      [](const std::vector<cv::Point2f>& a, const std::vector<cv::Point2f>& b) {
                          if (a.empty() || b.empty()) return false;
                          return a[0].y < b[0].y;
                      });
        }

    private:
        std::unique_ptr<TextDetector> detector_;
        std::unique_ptr<TextRecognizer> recognizer_;
        std::unique_ptr<TextClassifier> classifier_;
        float drop_score_;
        bool use_gpu_;
        bool save_crops_ = true;
        int crop_counter_ = 0;
    };

    TextSystem::TextSystem(const std::string& det_model_path,
                           const std::string& rec_model_path,
                           const std::string& rec_dict_path,
                           const std::string& orientation_model_path,
                           bool use_gpu)
            : pImpl(std::make_unique<Impl>(det_model_path, rec_model_path, rec_dict_path,
                                           orientation_model_path, use_gpu)) {}

    TextSystem::~TextSystem() = default;
    std::vector<OCRResult> TextSystem::recognizeCrop(const cv::Mat& img) {
        return pImpl->recognizeCrop(img);}
    std::vector<OCRResult> TextSystem::recognize(const cv::Mat& img) {
        return pImpl->recognize(img);
    }

} // namespace ocr