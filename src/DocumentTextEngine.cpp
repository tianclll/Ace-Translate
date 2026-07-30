#include "docmind/DocumentTextEngine.hpp"
#include "docmind/modules/FileTranslationModule.hpp"

#include <windows.h>
#include <cstring>
#include <fstream>
#include <stdexcept>

// 这些函数在 DocumentEngine.h 中声明在全局作用域（不在 namespace docmind 内）

std::string process_markdown(const std::string& md_path,
                             const std::string& base_dir,
                             const std::string& target_language) {
    docmind::FileTranslationModule module(target_language);
    return module.process(md_path, "");
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

std::string extract_file_text(const std::string& file_path,
                              const std::string& output_path,
                              const std::string& base_dir,
                              float layout_threshold,
                              int pdf_dpi,
                              bool enable_warp,
                              bool enable_enhance) {
    // 直接在当前线程调用（不创建嵌套线程），try/catch 保护
    try {
        docmind::FileTranslationModule module("", enable_warp, enable_enhance, layout_threshold, pdf_dpi, false);
        std::string result_path = module.process(file_path, output_path);
        // module.process() 返回的是输出文件路径，需要读取文件内容
        if (!result_path.empty()) {
            // 转换为 wide path 以支持中文路径
            int wlen = MultiByteToWideChar(CP_UTF8, 0, result_path.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                wchar_t* wpath = new wchar_t[wlen];
                MultiByteToWideChar(CP_UTF8, 0, result_path.c_str(), -1, wpath, wlen);
                FILE* f = _wfopen(wpath, L"rb");
                delete[] wpath;
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    std::string content;
                    content.resize(size);
                    fread(&content[0], 1, size, f);
                    fclose(f);
                    return content;
                }
            }
        }
        return "";
    } catch (...) {
        return "";
    }
}
