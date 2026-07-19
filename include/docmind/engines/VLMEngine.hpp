#pragma once

#include "DLLLoader.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace docmind {

    class VLMEngine {
    public:
        VLMEngine(DLLLoader& loader,
                  const std::string& model_path,
                  const std::string& mmproj_path,
                  int n_threads = 16,
                  int n_ctx = 8192,
                  int n_gpu_layers = 99);
        ~VLMEngine();

        bool isLoaded() const { return handle_ != nullptr; }

        // 单图识别
        std::string recognize(const cv::Mat& image,
                              const std::string& prompt,
                              int max_tokens = 512);

        // 批量识别：内部拼接并调用一次 VLM，返回按编号顺序的结果
        std::vector<std::string> batchRecognize(const std::vector<cv::Mat>& images,
                                                const std::string& prompt,
                                                int max_tokens = 512);

        // 获取最后错误（若支持）
        std::string getLastError() const;

    private:
        DLLLoader& dll_;
        void* handle_ = nullptr;
        bool initialized_ = false;
    };

} // namespace docmind