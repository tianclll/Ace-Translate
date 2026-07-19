#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/utils/FontManager.hpp"
#include "docmind/utils/ImageLayout.hpp"
#include "docmind/utils/ImageRenderer.hpp"

namespace docmind {

    class PhotoTranslationModule {
    public:
        /**
         * @brief 构造函数
         * @param target_language 目标语言（如 "English"、"中文"）
         * @param font_path 字体文件路径（用于渲染中文字符），若为空则使用系统默认
         * @param scale_factor 文本框扩大系数（默认1.2）
         */
        PhotoTranslationModule(
                const std::string& target_language = "English",
                const std::string& font_path = "",
                float scale_factor = 1.2f
        );

        /**
         * @brief 处理图像，返回翻译后的图像
         * @param input 输入图像
         * @param max_tokens 翻译最大token数
         * @return 翻译后图像
         */
        cv::Mat process(const cv::Mat& input, int max_tokens = 512);

    private:
        GlobalEngineContext& ctx_;
        std::string target_language_;
        float scale_factor_;
        std::unique_ptr<FontManager> font_manager_;
        std::unique_ptr<ImageLayout> layout_;
        std::unique_ptr<ImageRenderer> renderer_;
    };

} // namespace docmind