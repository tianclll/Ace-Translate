#pragma once

#include "docmind/types.hpp"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/utils/ImageUtils.hpp"
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

namespace docmind {

    struct ProcessorConfig {
        std::string assets_dir = "assets";
        bool enable_warp = true;
        bool enable_enhance = false;
        // 可选：其他配置
    };

    class DocumentProcessor {
    public:
        DocumentProcessor(GlobalEngineContext& ctx, const ProcessorConfig& config);
        ~DocumentProcessor();

        std::vector<std::string> process(const cv::Mat& image, float score_threshold = 0.5f);

    private:
        std::string processTextBlock(const cv::Mat& image,
                                     const BoxInfo& text_box,
                                     const std::vector<BoxInfo>& formula_boxes);
        std::string recognizeFormula(const cv::Mat& roi);
        bool isTextType(const std::string& label) const;
        bool isTitleType(const std::string& label) const;

        GlobalEngineContext& ctx_;
        ProcessorConfig config_;
        int image_counter_ = 0;
    };

} // namespace docmind