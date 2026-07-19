// src/predict_det.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "predict_det.hpp"
#include "db_postprocess.hpp"
#include <algorithm>
#include <numeric>
#include <codecvt>
#include <locale>
#include <iostream>
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

    TextDetector::TextDetector(const std::string& model_path, bool use_gpu) {


        sessionOptions = Ort::SessionOptions();
        sessionOptions.SetIntraOpNumThreads(4);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        sessionOptions.EnableCpuMemArena();
        sessionOptions.EnableMemPattern();

        if (use_gpu) {
            // 尝试附加 CUDA provider，失败则静默降级到 CPU
            try {
                OrtCUDAProviderOptions cudaOption;
                sessionOptions.AppendExecutionProvider_CUDA(cudaOption);
                std::cout << "  Inference device: GPU" << std::endl;
            } catch (const std::exception&) {
                std::cout << "  CUDA not available, using CPU" << std::endl;
            }
        } else {
            std::cout << "  Inference device: CPU" << std::endl;
        }

        std::wstring wpath = to_wstring(model_path);

        this->session = Ort::Session(env, wpath.c_str(), sessionOptions);


        auto input_info = this->session.GetInputTypeInfo(0);
        auto input_shape = input_info.GetTensorTypeAndShapeInfo().GetShape();
        this->inputShape = input_shape;

        auto output_info = this->session.GetOutputTypeInfo(0);
        auto output_shape = output_info.GetTensorTypeAndShapeInfo().GetShape();

    }

    TextDetector::~TextDetector() = default;

    cv::Mat TextDetector::preprocess(
            const cv::Mat& img,
            float& scale_x,
            float& scale_y,
            int& new_w,
            int& new_h) {

        int h = img.rows;
        int w = img.cols;
        int limit_side_len = input_size_;  // 736

        auto start = std::chrono::high_resolution_clock::now();

        // ============================================================
        // 1. 计算缩放比例 - 与 Python 完全一致
        // ============================================================
        float ratio = 1.0f;
        int min_side = (std::min)(h, w);

        if (min_side < limit_side_len) {
            if (h < w) {
                ratio = static_cast<float>(limit_side_len) / h;
            } else {
                ratio = static_cast<float>(limit_side_len) / w;
            }
        } else {
            ratio = 1.0f;
        }


        // ============================================================
        // 2. 计算新的尺寸
        // ============================================================
        int resize_h = static_cast<int>(h * ratio);
        int resize_w = static_cast<int>(w * ratio);

        // 调整为32的倍数 (与 Python 一致)
        new_h = (std::max)((resize_h / 32) * 32, 32);
        new_w = (std::max)((resize_w / 32) * 32, 32);

        // ============================================================
        // 3. 缩放
        // ============================================================
        cv::Mat resized;
        cv::resize(img, resized, cv::Size(new_w, new_h));

        // ============================================================
        // 4. 归一化 (与 Python 一致)
        // ============================================================
        cv::Mat float_img;
        resized.convertTo(float_img, CV_32FC3, 1.0 / 255.0);

        cv::Scalar mean(0.485f, 0.456f, 0.406f);
        cv::Scalar std(0.229f, 0.224f, 0.225f);
        cv::subtract(float_img, mean, float_img);
        cv::divide(float_img, std, float_img);

        // ============================================================
        // 5. 计算缩放比例 (用于还原到原图)
        // ============================================================
        scale_x = static_cast<float>(w) / new_w;
        scale_y = static_cast<float>(h) / new_h;

        auto end = std::chrono::high_resolution_clock::now();


        return float_img;
    }

    std::vector<std::vector<cv::Point2f>> TextDetector::detect(const cv::Mat& img) {
        auto total_start = std::chrono::high_resolution_clock::now();

        try {
            float scale_x;
            float scale_y;
            int new_w;
            int new_h;

            // ==========================
            // 1. 预处理
            // ==========================
            cv::Mat input_tensor = preprocess(img, scale_x, scale_y, new_w, new_h);

            if (input_tensor.empty()) {
                std::cerr << "  Preprocessing failed!" << std::endl;
                return {};
            }

            if (!input_tensor.isContinuous()) {
                input_tensor = input_tensor.clone();
            }

            size_t data_size = 3 * input_tensor.rows * input_tensor.cols;

            std::vector<float> input_data(data_size);

            auto convert_start = std::chrono::high_resolution_clock::now();

            // HWC -> CHW
            size_t idx = 0;
            for (int c = 0; c < 3; ++c) {
                for (int h = 0; h < input_tensor.rows; ++h) {
                    const cv::Vec3f* row = input_tensor.ptr<cv::Vec3f>(h);
                    for (int w = 0; w < input_tensor.cols; ++w) {
                        input_data[idx++] = row[w][c];
                    }
                }
            }

            auto convert_end = std::chrono::high_resolution_clock::now();

            // ==========================
            // 2. 输入shape
            // ==========================
            std::vector<int64_t> input_shape = {1, 3, input_tensor.rows, input_tensor.cols};

            // ==========================
            // 3. 创建 MemoryInfo
            // ==========================
            auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);


            // ==========================
            // 4. 创建 Tensor
            // ==========================
            Ort::Value input_tensor_ort = Ort::Value::CreateTensor<float>(
                    memory_info,
                    input_data.data(),
                    input_data.size(),
                    input_shape.data(),
                    input_shape.size()
            );


            // ==========================
            // 5. 获取输入输出名称
            // ==========================
            auto input_name = session.GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
            auto output_name = session.GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());

            const char* input_names[] = {input_name.get()};
            const char* output_names[] = {output_name.get()};

            // ==========================
            // 6. 推理
            // ==========================

            auto infer_start = std::chrono::high_resolution_clock::now();

            auto output_tensors = session.Run(
                    Ort::RunOptions{nullptr},
                    input_names,
                    &input_tensor_ort,
                    1,
                    output_names,
                    1
            );

            auto infer_end = std::chrono::high_resolution_clock::now();

            if (output_tensors.empty()) {
                std::cerr << "  No output tensors!" << std::endl;
                return {};
            }

            // ==========================
            // 7. 处理输出
            // ==========================
            auto& output = output_tensors[0];
            auto output_shape = output.GetTensorTypeAndShapeInfo().GetShape();

            const float* output_data = output.GetTensorData<float>();
            if (!output_data) {
                std::cerr << "  Output data is null!" << std::endl;
                return {};
            }

            int out_h = static_cast<int>(output_shape[2]);
            int out_w = static_cast<int>(output_shape[3]);

            // 检查输出值范围
            float min_val = 1e10f, max_val = -1e10f;
            size_t total_pixels = out_h * out_w;
            for (size_t i = 0; i < total_pixels; ++i) {
                float val = output_data[i];
                if (val < min_val) min_val = val;
                if (val > max_val) max_val = val;
            }

            cv::Mat output_mat(out_h, out_w, CV_32F);
            memcpy(output_mat.data, output_data, total_pixels * sizeof(float));

            // ==========================
            // 8. 后处理
            // ==========================

            DBPostProcess postprocess(
                    db_thresh_,
                    box_thresh_,
                    unclip_ratio_,
                    max_candidates_
            );

            auto boxes = postprocess.process(
                    output_mat,
                    img.cols,
                    img.rows,
                    scale_x,
                    scale_y
            );

            auto total_end = std::chrono::high_resolution_clock::now();

            // 打印每个框的大小
            for (size_t i = 0; i < boxes.size(); ++i) {
                float min_x = 1e10f, min_y = 1e10f, max_x = -1e10f, max_y = -1e10f;
                for (const auto& p : boxes[i]) {
                    if (p.x < min_x) min_x = p.x;
                    if (p.y < min_y) min_y = p.y;
                    if (p.x > max_x) max_x = p.x;
                    if (p.y > max_y) max_y = p.y;
                }

            }

            return boxes;
        }
        catch (const Ort::Exception& e) {
            std::cerr << "  ONNXRuntime ERROR: " << e.what() << std::endl;
            return {};
        }
        catch (const std::exception& e) {
            std::cerr << "  STD ERROR: " << e.what() << std::endl;
            return {};
        }
        catch (...) {
            std::cerr << "  UNKNOWN ERROR" << std::endl;
            return {};
        }
    }

} // namespace ocr