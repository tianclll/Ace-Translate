#include "docmind/engines/VLMEngine.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

namespace docmind {

    VLMEngine::VLMEngine(DLLLoader& loader,
                         const std::string& model_path,
                         const std::string& mmproj_path,
                         int n_threads,
                         int n_ctx,
                         int n_gpu_layers)
            : dll_(loader) {
        if (!dll_.vlmLoaded()) {
            throw std::runtime_error("VLM DLL not loaded");
        }
        if (!dll_.vlm_create || !dll_.vlm_destroy ||
            !dll_.vlm_understand_from_memory || !dll_.vlm_free_string) {
            throw std::runtime_error("VLM function pointers missing");
        }
        handle_ = dll_.vlm_create(model_path.c_str(), mmproj_path.c_str(), n_threads, n_ctx, n_gpu_layers);
        if (!handle_) {
            throw std::runtime_error("Failed to create VLM instance with model: " + model_path);
        }
        initialized_ = true;
        std::cout << "VLMEngine initialized." << std::endl;
    }

    VLMEngine::~VLMEngine() {
        if (handle_ && dll_.vlm_destroy) {
            dll_.vlm_destroy(handle_);
            handle_ = nullptr;
        }
    }

    std::string VLMEngine::recognize(const cv::Mat& image,
                                     const std::string& prompt,
                                     int max_tokens) {
        if (!initialized_ || image.empty()) return "";

        std::vector<uchar> img_buf;
        cv::imencode(".jpg", image, img_buf);

        const char* result = dll_.vlm_understand_from_memory(
                handle_,
                img_buf.data(),
                img_buf.size(),
                prompt.c_str(),
                max_tokens
        );
        std::string output;
        if (result) {
            output = result;
            dll_.vlm_free_string(result);
        } else {
            std::cerr << "VLM recognize failed: " << getLastError() << std::endl;
        }
        return output;
    }

    std::vector<std::string> VLMEngine::batchRecognize(const std::vector<cv::Mat>& images,
                                                       const std::string& prompt,
                                                       int max_tokens) {
        std::vector<std::string> results(images.size(), "");
        if (images.empty() || !initialized_) return results;

        // 如果只有一张图，直接调用单图识别
        if (images.size() == 1) {
            results[0] = recognize(images[0], prompt, max_tokens);
            return results;
        }

        // ---------- 拼接多图 ----------
        const int TARGET_HEIGHT = 80;
        const int GAP = 20;
        const int INDEX_WIDTH = 50;

        std::vector<cv::Mat> scaled;
        int total_width = 0;
        int max_h = 0;

        for (const auto& img : images) {
            if (img.empty()) {
                cv::Mat placeholder(TARGET_HEIGHT, TARGET_HEIGHT, CV_8UC3, cv::Scalar(255,255,255));
                scaled.push_back(placeholder);
                total_width += TARGET_HEIGHT + INDEX_WIDTH + GAP;
                max_h = std::max(max_h, TARGET_HEIGHT);
                continue;
            }
            float scale = (float)TARGET_HEIGHT / img.rows;
            int w = (int)(img.cols * scale);
            if (w < 1) w = 1;
            cv::Mat resized;
            cv::resize(img, resized, cv::Size(w, TARGET_HEIGHT));
            scaled.push_back(resized);
            total_width += w + INDEX_WIDTH + GAP;
            max_h = std::max(max_h, TARGET_HEIGHT);
        }

        cv::Mat canvas(max_h, total_width, CV_8UC3, cv::Scalar(255,255,255));
        int x_offset = 10;
        for (size_t i = 0; i < scaled.size(); ++i) {
            const auto& img = scaled[i];
            int y_offset = (canvas.rows - img.rows) / 2;
            cv::Rect roi_rect(x_offset + INDEX_WIDTH, y_offset, img.cols, img.rows);
            img.copyTo(canvas(roi_rect));
            x_offset += INDEX_WIDTH + img.cols + GAP;
        }

        // 调用 VLM 识别拼接图
        std::string raw = recognize(canvas, prompt, max_tokens);
        if (raw.empty()) {
            // 降级：逐个识别
            for (size_t i = 0; i < images.size(); ++i) {
                results[i] = recognize(images[i], prompt, max_tokens);
            }
            return results;
        }

        // 解析 <fcel> 分隔
        std::vector<std::string> parts;
        std::string delimiter = "<fcel>";
        size_t pos = 0;
        std::string remaining = raw;
        while ((pos = remaining.find(delimiter)) != std::string::npos) {
            std::string part = remaining.substr(0, pos);
//            // 去除首尾空白
//            size_t s = part.find_first_not_of(" \t\r\n");
//            size_t e = part.find_last_not_of(" \t\r\n");
//            if (s != std::string::npos && e != std::string::npos) {
//                part = part.substr(s, e - s + 1);
//            } else {
//                part = "";
//            }
            if (!part.empty()) {
                parts.push_back(part + " ");
            }
            remaining.erase(0, pos + delimiter.length());
        }
        // 最后一个片段
        if (!remaining.empty()) {
//            size_t s = remaining.find_first_not_of(" \t\r\n");
//            size_t e = remaining.find_last_not_of(" \t\r\n");
//            if (s != std::string::npos && e != std::string::npos) {
//                remaining = remaining.substr(s, e - s + 1);
//            } else {
//                remaining = "";
//            }
            if (!remaining.empty() && remaining != "<nl>") {
                parts.push_back(remaining);
            }
        }

//        // 按顺序填充结果
//        // 2. 清理每个片段：去掉末尾的数字（及前面的空格）
//        auto cleanPart = [](const std::string& s) -> std::string {
//            if (s.empty()) return "";
//            int i = (int)s.size() - 1;
//            // 从末尾开始跳过数字和空格
//            while (i >= 0 && (std::isdigit(s[i]) || s[i] == ' ')) {
//                i--;
//            }
//            if (i < 0) return "";  // 全是数字或空格
//            return s.substr(0, i + 1);
//        };
//
//        std::vector<std::string> cleaned;
//        for (const auto& part : parts) {
//            std::string cleaned_part = cleanPart(part);
//            if (!cleaned_part.empty()) {
//                cleaned.push_back(cleaned_part);
//            }
//        }

// 按顺序填充结果
        size_t count = std::min(parts.size(), results.size());
        for (size_t i = 0; i < count; ++i) {
            results[i] = parts[i];
        }
        // 未填的降级单独识别
        for (size_t i = count; i < results.size(); ++i) {
            if (results[i].empty() && !images[i].empty()) {
                results[i] = recognize(images[i], prompt, max_tokens);
            }
        }
        return results;
    }

    std::string VLMEngine::getLastError() const {
        if (handle_ && dll_.vlm_get_last_error) {
            const char* err = dll_.vlm_get_last_error(handle_);
            return err ? std::string(err) : "";
        }
        return "";
    }

} // namespace docmind