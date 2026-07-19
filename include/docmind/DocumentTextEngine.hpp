#pragma once

#include <string>

namespace docmind {

// ============================================================
// 文本文件翻译接口（兼容旧版，建议使用 DocumentEngine.h 中的统一接口）
// ============================================================

/**
 * @brief 翻译 Markdown 文件（保留格式）
 * @param md_path        Markdown文件路径
 * @param base_dir       资源目录（DLL和模型所在目录，默认自动获取可执行文件目录）
 * @param target_language 目标语言（如 "English"、"中文"）
 * @return 翻译后的 Markdown 字符串
 */
    std::string process_markdown(
            const std::string& md_path,
            const std::string& base_dir = "",
            const std::string& target_language = "English"
    );

/**
 * @brief 翻译纯文本文件（全文翻译）
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

} // namespace docmind