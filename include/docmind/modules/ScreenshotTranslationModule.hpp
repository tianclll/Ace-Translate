#pragma once

#include <string>
#include <opencv2/opencv.hpp>

namespace docmind {

/**
 * @brief 截图翻译模块：从图像中提取文字并翻译（返回纯文本）
 * 适用于截图、任意图片中的文字提取翻译，不修改原图。
 */
    class ScreenshotTranslationModule {
    public:
        /**
         * @brief 构造函数
         * @param target_language 目标语言，默认从配置读取
         */
        explicit ScreenshotTranslationModule(const std::string& target_language = "");

        /**
         * @brief 从图像中提取文字并翻译（返回纯文本）
         * @param image      输入图像
         * @param max_tokens 翻译最大token数
         * @return 图像中文字的翻译结果（纯文本）
         */
        std::string translate(const cv::Mat& image, int max_tokens = 512);

    private:
        std::string target_language_;
    };

} // namespace docmind