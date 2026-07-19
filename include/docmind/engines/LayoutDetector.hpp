#pragma once

#include "docmind/types.hpp"
#include "DLLLoader.hpp"
#include <opencv2/opencv.hpp>
#include <memory>

namespace docmind {

    class LayoutDetector {
    public:
        LayoutDetector(DLLLoader& loader, const std::string& model_path, bool use_gpu = true, int device_id = 0);
        ~LayoutDetector();

        // 检测版面，返回 BoxInfo 列表
        std::vector<BoxInfo> detect(const cv::Mat& image, float score_threshold = 0.5f);

    private:
        DLLLoader& dll_;           // 引用，生命周期由外部管理
        void* handle_ = nullptr;   // layout 实例句柄
        bool initialized_ = false;
    };

} // namespace docmind