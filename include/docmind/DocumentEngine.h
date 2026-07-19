#pragma once
#include <string>
#include <vector>
#include "opencv2/opencv.hpp"
// ============================================================
// 对外统一接口（兼容旧版，同时支持新模块）
// ============================================================

/**
 * @brief 处理图片文件（使用文档图片流程：版面分析 + OCR + VLM）
 * @param image_path      图片路径
 * @param base_dir        资源目录（DLL和模型所在目录，默认自动获取可执行文件目录）
 * @param target_language 目标语言（如 "English"、"中文"）
 * @param layout_threshold 版面检测置信度阈值（0~1）
 * @return 翻译后的 Markdown 字符串
 */
std::string process_image(
        const std::string& image_path,
        const std::string& base_dir = "",
        const std::string& target_language = "English",
        float layout_threshold = 0.5f
);

/**
 * @brief 处理 PDF 文件（逐页渲染为图片，然后使用文档图片流程）
 * @param pdf_path        PDF路径
 * @param base_dir        资源目录
 * @param target_language 目标语言
 * @param layout_threshold 版面检测阈值
 * @param dpi             PDF渲染DPI（默认200）
 * @return 翻译后的 Markdown 字符串
 */
std::string process_pdf(
        const std::string& pdf_path,
        const std::string& base_dir = "",
        const std::string& target_language = "English",
        float layout_threshold = 0.5f,
        int dpi = 200
);

/**
 * @brief 处理 Markdown 文件（翻译其中的自然语言文本，保留格式）
 * @param md_path        Markdown文件路径
 * @param base_dir       资源目录
 * @param target_language 目标语言
 * @return 翻译后的 Markdown 字符串
 */
std::string process_markdown(
        const std::string& md_path,
        const std::string& base_dir = "",
        const std::string& target_language = "English"
);

/**
 * @brief 处理纯文本文件（全文翻译）
 * @param txt_path        文本文件路径
 * @param base_dir        资源目录
 * @param target_language 目标语言
 * @return 翻译后的纯文本字符串
 */
std::string process_txt(
        const std::string& txt_path,
        const std::string& base_dir = "",
        const std::string& target_language = "English"
);

/**
 * @brief 统一文件处理入口（自动根据扩展名路由，支持图片/PDF/Office/MD/TXT）
 * @param file_path        输入文件路径
 * @param output_path      输出文件路径（若为空则自动生成）
 * @param base_dir         资源目录
 * @param target_language  目标语言
 * @param layout_threshold 版面检测阈值（仅对图片/PDF有效）
 * @param pdf_dpi          PDF渲染DPI
 * @param enable_warp      是否启用去扭曲（仅对文档图片有效）
 * @param enable_enhance   是否启用图像增强（仅对文档图片有效）
 * @return 实际写入的输出文件路径
 */
std::string process_file(
        const std::string& file_path,
        const std::string& output_path = "",
        const std::string& base_dir = "",
        const std::string& target_language = "English",
        float layout_threshold = 0.5f,
        int pdf_dpi = 200,
        bool enable_warp = true,
        bool enable_enhance = false
);

/**
 * @brief 普通图片翻译（仅 VLM，不进行版面分析）——暂未实现，预留
 */
std::string process_photo(
        const std::string& image_path,
        const std::string& output_path = "",
        const std::string& base_dir = "",
        const std::string& target_language = "English"
);

/**
 * @brief 普通图片翻译（仅VLM，不进行版面分析）——使用OCR+渲染
 * @param image_path     输入图片路径
 * @param output_path    输出图片路径（若为空，则自动生成在原图同目录，添加 "_trans" 后缀）
 * @param base_dir       资源目录
 * @param target_language 目标语言
 * @param max_tokens     翻译最大token数
 * @return 实际写入的输出图片路径
 */
std::string process_photo(
        const std::string& image_path,
        const std::string& output_path = "",
        const std::string& base_dir = "",
        const std::string& target_language = "English",
        int max_tokens = 512
);
/**
 * @brief 纯文本翻译
 * @param text           待翻译文本
 * @param target_language 目标语言（如 "English"、"中文"）
 * @param max_tokens     最大token数
 * @return 翻译后的文本
 */
std::string translate_text(
        const std::string& text,
        const std::string& target_language = "English",
        int max_tokens = 512
);

/**
 * @brief 从图像中提取文字并翻译（返回纯文本）
 * @param image          输入图像（如截图）
 * @param target_language 目标语言
 * @param max_tokens     最大token数
 * @return 图像中文字的翻译结果
 */
std::string translate_screenshot_image(
        const cv::Mat& image,
        const std::string& target_language = "English",
        int max_tokens = 512
);