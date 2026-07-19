// src/predict_cls.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "predict_cls.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ocr {

    static std::wstring to_wstring(const std::string& str) {
#ifdef _WIN32
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (len <= 0) return std::wstring();
        std::wstring wstr(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        wstr.pop_back();
        return wstr;
#else
        return std::wstring(str.begin(), str.end());
#endif
    }

    TextClassifier::TextClassifier(const std::string& model_path, bool use_gpu) {

        sessionOptions = Ort::SessionOptions();
        sessionOptions.SetIntraOpNumThreads(4);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        if (use_gpu) {
            try {
                OrtCUDAProviderOptions cuda_option;
                sessionOptions.AppendExecutionProvider_CUDA(cuda_option);
                std::cout << "  Classification using GPU" << std::endl;
            } catch (const std::exception&) {
                std::cout << "  CUDA not available, classification using CPU" << std::endl;
            }
        } else {
            std::cout << "  Classification using CPU" << std::endl;
        }

        std::wstring wpath = to_wstring(model_path);
        session = Ort::Session(env, wpath.c_str(), sessionOptions);
        memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        // 打印输入输出信息
        auto input_info = session.GetInputTypeInfo(0);
        auto input_shape = input_info.GetTensorTypeAndShapeInfo().GetShape();

        auto output_info = session.GetOutputTypeInfo(0);
        auto output_shape = output_info.GetTensorTypeAndShapeInfo().GetShape();
    }

    TextClassifier::~TextClassifier() = default;

    cv::Mat TextClassifier::resizeNormImg(const cv::Mat& img) {
        int imgC = cls_image_shape_[0];  // 3
        int imgH = cls_image_shape_[1];  // 48
        int imgW = cls_image_shape_[2];  // 192

        int h = img.rows;
        int w = img.cols;

        float ratio = static_cast<float>(w) / h;
        int resized_w;

        if (std::ceil(imgH * ratio) > imgW) {
            resized_w = imgW;
        } else {
            resized_w = static_cast<int>(std::ceil(imgH * ratio));
        }


        // resize
        cv::Mat resized_image;
        cv::resize(img, resized_image, cv::Size(resized_w, imgH), 0, 0, cv::INTER_LINEAR);

        // 转换为float32并归一化
        cv::Mat float_img;
        resized_image.convertTo(float_img, CV_32FC3, 1.0 / 255.0);

        // 转CHW
        std::vector<cv::Mat> channels(3);
        cv::split(float_img, channels);

        cv::Mat chw(3, imgH * resized_w, CV_32F);
        for (int c = 0; c < 3; c++) {
            memcpy(chw.ptr<float>(c), channels[c].data, imgH * resized_w * sizeof(float));
        }

        // 减去0.5，除以0.5
        cv::Mat normalized = chw.clone();
        cv::subtract(normalized, cv::Scalar(0.5f), normalized);
        cv::divide(normalized, cv::Scalar(0.5f), normalized);

        // padding
        cv::Mat padding = cv::Mat::zeros(imgC, imgH * imgW, CV_32F);
        for (int c = 0; c < imgC; c++) {
            memcpy(padding.ptr<float>(c), normalized.ptr<float>(c), imgH * resized_w * sizeof(float));
        }

        return padding;
    }

    std::pair<std::vector<cv::Mat>, std::vector<std::vector<std::string>>>
    TextClassifier::classify(const std::vector<cv::Mat>& images) {


        std::vector<cv::Mat> img_list = images;
        size_t img_num = img_list.size();
        std::vector<std::vector<std::string>> cls_res(img_num, {"", "0.0"});

        // 按宽高比排序
        std::vector<float> width_list;
        for (const auto& img : img_list) {
            width_list.push_back(static_cast<float>(img.cols) / img.rows);
        }

        std::vector<int> indices(img_num);
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&](int i, int j) {
            return width_list[i] < width_list[j];
        });

        auto input_name = session.GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto output_name = session.GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());

        const char* input_names[] = {input_name.get()};
        const char* output_names[] = {output_name.get()};

        for (size_t beg = 0; beg < img_num; beg += cls_batch_num_) {
            size_t end = std::min(beg + cls_batch_num_, img_num);
            size_t batch = end - beg;


            // 计算最大宽高比
            float max_wh_ratio = 0.0f;
            for (size_t i = beg; i < end; i++) {
                int idx = indices[i];
                float wh_ratio = static_cast<float>(img_list[idx].cols) / img_list[idx].rows;
                max_wh_ratio = std::max(max_wh_ratio, wh_ratio);
            }

            // 预处理batch
            std::vector<cv::Mat> batch_norm_imgs;
            for (size_t i = beg; i < end; i++) {
                int idx = indices[i];
                cv::Mat norm_img = resizeNormImg(img_list[idx]);
                batch_norm_imgs.push_back(norm_img);
            }

            int imgC = cls_image_shape_[0];
            int imgH = cls_image_shape_[1];
            int imgW = cls_image_shape_[2];

            std::vector<float> batch_data(batch * imgC * imgH * imgW);

            for (size_t i = 0; i < batch; i++) {
                memcpy(
                        batch_data.data() + i * imgC * imgH * imgW,
                        batch_norm_imgs[i].data,
                        imgC * imgH * imgW * sizeof(float)
                );
            }

            std::vector<int64_t> input_shape = {
                    static_cast<int64_t>(batch),
                    imgC,
                    imgH,
                    imgW
            };


            auto input_tensor = Ort::Value::CreateTensor<float>(
                    memoryInfo,
                    batch_data.data(),
                    batch_data.size(),
                    input_shape.data(),
                    input_shape.size()
            );

            auto start = std::chrono::high_resolution_clock::now();

            auto outputs = session.Run(
                    Ort::RunOptions{nullptr},
                    input_names,
                    &input_tensor,
                    1,
                    output_names,
                    1
            );

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start);

            auto& output = outputs[0];
            const float* prob_out = output.GetTensorData<float>();
            auto output_shape = output.GetTensorTypeAndShapeInfo().GetShape();

            int num_classes = static_cast<int>(output_shape[1]);

            for (size_t offset = 0; offset < batch; offset++) {
                size_t idx = beg + offset;
                size_t original_idx = indices[idx];

                const float* prob = prob_out + offset * num_classes;

                int max_idx = 0;
                float max_score = prob[0];
                for (int c = 1; c < num_classes; c++) {
                    if (prob[c] > max_score) {
                        max_score = prob[c];
                        max_idx = c;
                    }
                }

                std::string label = (max_idx < static_cast<int>(label_list_.size()))
                                    ? label_list_[max_idx]
                                    : std::to_string(max_idx);
                float score = max_score;

                cls_res[original_idx] = {label, std::to_string(score)};

                // 如果标签包含"180"且分数大于阈值，旋转图像
                if (label.find("180") != std::string::npos && score > cls_thresh_) {
                    cv::rotate(img_list[original_idx], img_list[original_idx], cv::ROTATE_180);
                }
            }
        }

        return {img_list, cls_res};
    }

} // namespace ocr