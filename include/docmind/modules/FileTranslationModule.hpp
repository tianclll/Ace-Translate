#pragma once

#include <string>

namespace docmind {

    class FileTranslationModule {
    public:
        /**
         * @brief 构造函数
         * @param target_language 目标语言（如 "English"、"中文"）
         * @param enable_warp     是否启用去扭曲（仅对文档图片有效）
         * @param enable_enhance  是否启用图像增强（仅对文档图片有效）
         * @param layout_threshold 版面检测阈值（0~1）
         * @param pdf_dpi         PDF渲染DPI
         */
        FileTranslationModule(
                const std::string& target_language = "",   // 空表示使用配置默认
                bool enable_warp = true,
                bool enable_enhance = false,
                float layout_threshold = 0.5f,
                int pdf_dpi = 200
        );

        /**
         * @brief 处理文件，返回输出文件路径
         * @param input_path  输入文件路径
         * @param output_path 输出文件路径（若为空，自动生成）
         * @return 实际写入的文件路径
         */
        std::string process(const std::string& input_path, const std::string& output_path = "");

    private:
        std::string target_language_;
        bool enable_warp_;
        bool enable_enhance_;
        float layout_threshold_;
        int pdf_dpi_;
    };

} // namespace docmind