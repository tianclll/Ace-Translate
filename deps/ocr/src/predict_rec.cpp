// src/predict_rec.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "predict_rec.hpp"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ocr {

// ============================================================
// 辅助函数：UTF-8转宽字符串
// ============================================================
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

// ============================================================
// 构造函数
// ============================================================
    TextRecognizer::TextRecognizer(const std::string& model_path,
                                   const std::string& dict_path,
                                   bool use_gpu,
                                   int max_text_length)
            : max_text_length_(max_text_length) {   // 初始化成员

        // 加载字典
        loadDict(dict_path);

        // 设置Session选项
        sessionOptions = Ort::SessionOptions();
        sessionOptions.SetIntraOpNumThreads(4);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // GPU支持
        if (use_gpu) {
            try {
                OrtCUDAProviderOptions cuda_option;
                sessionOptions.AppendExecutionProvider_CUDA(cuda_option);
                std::cout << "  Recognition using GPU" << std::endl;
            } catch (const std::exception&) {
                std::cout << "  CUDA not available, recognition using CPU" << std::endl;
            }
        } else {
            std::cout << "  Recognition using CPU" << std::endl;
        }

        // 创建Session
        std::wstring wpath = to_wstring(model_path);
        session = Ort::Session(env, wpath.c_str(), sessionOptions);
        memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        // 获取输入形状
        inputShape = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();


    }

    TextRecognizer::~TextRecognizer() = default;

// ============================================================
// 加载字典
// ============================================================
    void TextRecognizer::loadDict(const std::string& dict_path) {
        char_list_.clear();
        std::ifstream file(dict_path);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open dict: " + dict_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            // 去除BOM
            if (!line.empty() && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
                line = line.substr(3);
            }
            // 去除\r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                char_list_.push_back(line);
            }
        }
        file.close();
    }

// ============================================================
// 预处理：只做resize（与正常工作的代码完全一致）
// ============================================================
    cv::Mat TextRecognizer::preprocess(const cv::Mat& img, float max_wh_ratio) {
        int imgH = imgH_;
        int imgW = static_cast<int>(imgH * max_wh_ratio);
        if (imgW < 1) imgW = 1;

        int h = img.rows;
        int w = img.cols;

        float ratio = (float)w / h;
        int resized_w = static_cast<int>(std::ceil(imgH * ratio));

        if (resized_w > imgW) {
            resized_w = imgW;
        }
        if (resized_w < 1) resized_w = 1;

        // 只做resize，不做归一化（与正常工作的代码一致）
        cv::Mat resized;
        cv::resize(img, resized, cv::Size(resized_w, imgH), 0, 0, cv::INTER_LINEAR);


        return resized;
    }

// ============================================================
// 归一化：将resize后的图像转换为模型输入（与正常工作的代码完全一致）
// ============================================================
    std::vector<float> TextRecognizer::normalize(const cv::Mat& img, int imgW) {
        int row = img.rows;      // 48
        int col = img.cols;      // resized_w
        int channels = img.channels();  // 3

        std::vector<float> input_data(3 * row * imgW, 0.0f);

        // 与正常工作的代码完全一致：CHW格式，c * row * imgW + i * imgW + j
        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < row; i++) {
                for (int j = 0; j < imgW; j++) {
                    if (j < col) {
                        // img是BGR格式
                        float pix = img.ptr<uchar>(i)[j * 3 + c];
                        // (pix / 255.0 - 0.5) / 0.5
                        input_data[c * row * imgW + i * imgW + j] = (pix / 255.0f - 0.5f) / 0.5f;
                    } else {
                        input_data[c * row * imgW + i * imgW + j] = 0.0f;
                    }
                }
            }
        }

        return input_data;
    }

// ============================================================
// CTC解码
// ============================================================
    std::string TextRecognizer::ctcDecode(const std::vector<int>& pred_indices, int max_length) {
        std::string result;
        int last_idx = -1;
        for (int idx : pred_indices) {
            if (idx == 0) { last_idx = 0; continue; }   // skip blank
            if (idx == last_idx) continue;              // skip repeat
            last_idx = idx;
            int dict_idx = idx - 1;
            if (dict_idx >= 0 && dict_idx < static_cast<int>(char_list_.size())) {
                result += char_list_[dict_idx];
            }
        }
        return result;
    }

// ============================================================
// 识别主函数
// ============================================================
    std::vector<std::pair<std::string, float>> TextRecognizer::recognize(
            const std::vector<cv::Mat>& images) {

        std::vector<std::pair<std::string, float>> results(images.size());

        if (images.empty()) {
            return results;
        }

        // 按宽高比排序（与Python一致）
        std::vector<int> indices(images.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
                  [&](int i, int j) {
                      float r1 = (float)images[i].cols / images[i].rows;
                      float r2 = (float)images[j].cols / images[j].rows;
                      return r1 < r2;
                  });

        // 获取输入输出名称
        auto input_name = session.GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto output_name = session.GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());

        const char* input_names[] = {input_name.get()};
        const char* output_names[] = {output_name.get()};

        int imgH = imgH_;

        // 保存预处理图像计数器
        int preprocess_counter = 0;

        for (size_t start = 0; start < images.size(); start += batch_size_) {
            size_t end = std::min(start + batch_size_, images.size());
            size_t batch = end - start;

            // 计算动态宽度
//            float max_wh_ratio = 320.0f / 48.0f;
            float max_wh_ratio = 320.0f / 48.0f;
            for (size_t i = start; i < end; i++) {
                int idx = indices[i];
                float ratio = (float)images[idx].cols / images[idx].rows;
                max_wh_ratio = std::max(max_wh_ratio, ratio);
            }

            int dynamic_imgW = static_cast<int>(imgH * max_wh_ratio);
            if (dynamic_imgW < 1) dynamic_imgW = 1;

            // ============================================================
            // 1. 预处理：只做resize
            // ============================================================
            std::vector<cv::Mat> batch_resized;
            std::vector<int> batch_resized_w;

            for (size_t i = start; i < end; i++) {
                int idx = indices[i];
                cv::Mat resized = preprocess(images[idx], max_wh_ratio);
                batch_resized.push_back(resized);
                batch_resized_w.push_back(resized.cols);
            }

            // ============================================================
            // 2. 归一化：与正常工作的代码完全一致
            // ============================================================
            std::vector<float> input_data(batch * 3 * imgH * dynamic_imgW);

            for (size_t b = 0; b < batch; b++) {
                int resized_w = batch_resized_w[b];
                cv::Mat& img = batch_resized[b];

                for (int i = 0; i < imgH; i++) {
                    for (int j = 0; j < dynamic_imgW; j++) {
                        size_t base_idx = b * 3 * imgH * dynamic_imgW + i * dynamic_imgW + j;
                        if (j < resized_w) {
                            uchar* p = img.ptr<uchar>(i) + j * 3;
                            float r = (p[2] / 255.0f - 0.5f) / 0.5f;  // R
                            float g = (p[1] / 255.0f - 0.5f) / 0.5f;  // G
                            float b = (p[0] / 255.0f - 0.5f) / 0.5f;  // B
                            input_data[base_idx + 0 * imgH * dynamic_imgW] = r;
                            input_data[base_idx + 1 * imgH * dynamic_imgW] = g;
                            input_data[base_idx + 2 * imgH * dynamic_imgW] = b;
                        } else {
                            input_data[base_idx + 0 * imgH * dynamic_imgW] = 0.0f;
                            input_data[base_idx + 1 * imgH * dynamic_imgW] = 0.0f;
                            input_data[base_idx + 2 * imgH * dynamic_imgW] = 0.0f;
                        }
                    }
                }
            }

            // ============================================================
            // 3. 创建输入Tensor
            // ============================================================
            std::vector<int64_t> input_shape = {
                    (int64_t)batch, 3, imgH, dynamic_imgW
            };

            auto input_tensor = Ort::Value::CreateTensor<float>(
                    memoryInfo,
                    input_data.data(),
                    input_data.size(),
                    input_shape.data(),
                    input_shape.size()
            );

            // ============================================================
            // 4. 推理
            // ============================================================
            auto outputs = session.Run(
                    Ort::RunOptions{nullptr},
                    input_names,
                    &input_tensor,
                    1,
                    output_names,
                    1
            );

            // ============================================================
            // 5. 处理输出
            // ============================================================
            auto& output = outputs[0];
            auto shape = output.GetTensorTypeAndShapeInfo().GetShape();

            const float* out = output.GetTensorData<float>();

            int bs = static_cast<int>(shape[0]);
            int T = static_cast<int>(shape[1]);  // 序列长度
            int C = static_cast<int>(shape[2]);  // 字符类别数

            for (int b = 0; b < bs; b++) {
                // 获取当前batch的预测结果
                std::vector<int> pred_indices(T);
                float total_score = 0.0f;

                for (int t = 0; t < T; t++) {
                    const float* row = out + b * T * C + t * C;

                    int max_idx = 0;
                    float max_prob = row[0];

                    for (int c = 1; c < C; c++) {
                        if (row[c] > max_prob) {
                            max_prob = row[c];
                            max_idx = c;
                        }
                    }

                    pred_indices[t] = max_idx;
                    total_score += max_prob;
                }

                // CTC解码
                std::string text = ctcDecode(pred_indices, max_text_length_);

                // 计算置信度（平均概率）
                float confidence = total_score / T;

                results[indices[start + b]] = {text, confidence};

            }
        }

        return results;
    }

} // namespace ocr