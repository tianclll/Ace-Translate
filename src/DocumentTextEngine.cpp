#include "docmind/DocumentTextEngine.hpp"
#include "docmind/modules/FileTranslationModule.hpp"

// 这些函数在 DocumentEngine.h 中声明在全局作用域（不在 namespace docmind 内）

std::string process_markdown(const std::string& md_path,
                             const std::string& base_dir,
                             const std::string& target_language) {
    docmind::FileTranslationModule module(target_language);
    return module.process(md_path, ""); // 自动输出
}

std::string process_txt(const std::string& txt_path,
                        const std::string& base_dir,
                        const std::string& target_language) {
    docmind::FileTranslationModule module(target_language);
    return module.process(txt_path, "");
}

std::string process_file(const std::string& file_path,
                         const std::string& output_path,
                         const std::string& base_dir,
                         const std::string& target_language,
                         float layout_threshold,
                         int pdf_dpi,
                         bool enable_warp,
                         bool enable_enhance) {
    docmind::FileTranslationModule module(target_language, enable_warp, enable_enhance, layout_threshold, pdf_dpi);
    return module.process(file_path, output_path);
}