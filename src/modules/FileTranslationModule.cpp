#define _CRT_SECURE_NO_WARNINGS
#include "docmind/core/ConfigManager.hpp"
#include "docmind/modules/FileTranslationModule.hpp"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/processors/DocumentProcessor.hpp"
#include "docmind/utils/ImageUtils.hpp"
#include "docmind/utils/MarkdownGenerator.hpp"
#include "docmind/utils/OfficeConverter.h"
#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <Windows.h>
#include <fpdfview.h>
#include <fpdf_edit.h>
//#include <mutex>
#include <random>
#include <regex>
#include <unordered_map>

namespace docmind {
// 辅助函数：翻译 HTML 中的纯文本，保留所有标签
    static std::string translateHtmlContent(const std::string& html,
                                            TranslatorEngine& translator,
                                            const std::string& target_lang) {
        // 实现代码（使用占位符保护标签）
        std::regex tag_regex("<[^>]+>");
        std::string processed = html;
        std::unordered_map<std::string, std::string> placeholder_map;
        int counter = 0;

        // 1. 替换所有标签为占位符
        std::smatch match;
        while (std::regex_search(processed, match, tag_regex)) {
            std::string tag = match[0];
            std::string placeholder = "___TAG_" + std::to_string(counter++) + "___";
            placeholder_map[placeholder] = tag;
            processed = match.prefix().str() + placeholder + match.suffix().str();
        }

        // 2. 翻译纯文本（占位符之间的内容）
        std::regex placeholder_regex("___TAG_\\d+___");
        std::string translated;
        std::sregex_iterator it(processed.begin(), processed.end(), placeholder_regex);
        std::sregex_iterator end;
        size_t last_pos = 0;

        for (; it != end; ++it) {
            size_t pos = it->position();
            if (pos > last_pos) {
                std::string text = processed.substr(last_pos, pos - last_pos);
                if (!text.empty()) {
                    text = std::regex_replace(text, std::regex("^\\s+|\\s+$"), "");
                    if (!text.empty()) {
                        translated += translator.translate(text, target_lang);
                    }
                }
            }
            translated += it->str();
            last_pos = pos + it->length();
        }

        if (last_pos < processed.length()) {
            std::string remaining = processed.substr(last_pos);
            if (!remaining.empty()) {
                remaining = std::regex_replace(remaining, std::regex("^\\s+|\\s+$"), "");
                if (!remaining.empty()) {
                    translated += translator.translate(remaining, target_lang);
                }
            }
        }

        // 3. 还原标签
        for (const auto& pair : placeholder_map) {
            size_t pos = translated.find(pair.first);
            while (pos != std::string::npos) {
                translated.replace(pos, pair.first.length(), pair.second);
                pos = translated.find(pair.first, pos + pair.second.length());
            }
        }

        return translated;
    }
// ---------- 内部辅助函数 ----------

    static std::string get_exe_directory() {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exeDir(exePath);
        size_t pos = exeDir.find_last_of("\\/");
        if (pos != std::string::npos) exeDir = exeDir.substr(0, pos);
        return exeDir;
    }

// 支持中文路径读取图像
    static cv::Mat imread_unicode(const std::string& utf8_path) {
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8_path.c_str(), -1, nullptr, 0);
        if (len <= 0) return cv::Mat();
        std::wstring wpath(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8_path.c_str(), -1, &wpath[0], len);
        wpath.pop_back();

        FILE* file = _wfopen(wpath.c_str(), L"rb");
        if (!file) return cv::Mat();
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        std::vector<unsigned char> buffer(size);
        fread(buffer.data(), 1, size, file);
        fclose(file);
        return cv::imdecode(buffer, cv::IMREAD_COLOR);
    }
    static std::string get_temp_md_path(const std::string& dir) {
        std::string temp_dir = dir.empty() ? "." : dir;
        static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr) + GetCurrentProcessId()));
        std::uniform_int_distribution<int> dist(1000, 9999);
        std::string filename = "docmd_" + std::to_string(dist(rng)) + "_" + std::to_string(GetCurrentProcessId()) + ".md";
        std::string path = temp_dir + "\\" + filename;
        // 若已存在则删除（极小概率）
        std::remove(path.c_str());
        return path;
    }
// PDF 渲染为图像列表
    static std::vector<cv::Mat> pdf_to_images(const std::string& pdf_path, int dpi) {
        static bool initialized = false;
        if (!initialized) {
            FPDF_InitLibrary();
            initialized = true;
        }

        std::ifstream file(pdf_path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            throw std::runtime_error("Failed to open PDF file: " + pdf_path);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();

        FPDF_DOCUMENT doc = FPDF_LoadMemDocument(data.data(), static_cast<int>(size), nullptr);
        if (!doc) {
            unsigned long err = FPDF_GetLastError();
            throw std::runtime_error("Failed to load PDF. Error: " + std::to_string(err));
        }

        int page_count = FPDF_GetPageCount(doc);
        if (page_count <= 0) {
            FPDF_CloseDocument(doc);
            throw std::runtime_error("PDF has no pages.");
        }

        std::vector<cv::Mat> pages;
        for (int i = 0; i < page_count; ++i) {
            FPDF_PAGE page = FPDF_LoadPage(doc, i);
            if (!page) continue;

            double pw = FPDF_GetPageWidth(page);
            double ph = FPDF_GetPageHeight(page);
            int width = static_cast<int>(pw * dpi / 72.0);
            int height = static_cast<int>(ph * dpi / 72.0);

            FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 1);
            if (!bitmap) {
                FPDF_ClosePage(page);
                continue;
            }

            FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
            FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, 0);

            int stride = FPDFBitmap_GetStride(bitmap);
            void* buffer = FPDFBitmap_GetBuffer(bitmap);
            if (buffer) {
                cv::Mat mat4(height, width, CV_8UC4, buffer, stride);
                cv::Mat mat3;
                cv::cvtColor(mat4, mat3, cv::COLOR_BGRA2BGR);
                pages.push_back(mat3.clone());
            }

            FPDFBitmap_Destroy(bitmap);
            FPDF_ClosePage(page);
        }

        FPDF_CloseDocument(doc);
        if (pages.empty())
            throw std::runtime_error("No pages rendered from PDF.");
        return pages;
    }

// 自动生成输出路径
    static std::string auto_output_path(const std::string& input_path, const std::string& ext) {
        size_t slash = input_path.find_last_of("\\/");
        std::string dir = (slash != std::string::npos) ? input_path.substr(0, slash + 1) : "";
        std::string base = input_path.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        std::string out_ext = (ext == "txt") ? ".txt" : ".md";
        return dir + base + out_ext;
    }

// 创建目录（若不存在）
    static void create_directory(const std::string& path) {
        if (path.empty()) return;
        std::error_code ec;
        if (!std::filesystem::create_directories(path, ec) && ec)
            throw std::runtime_error("Failed to create directory: " + path);
    }

// ---------- 翻译文本辅助（使用全局 translator） ----------
    static std::string translate_text(const std::string& text, const std::string& target_lang, int max_tokens = 512) {
        // 使用新重载
        if (text.empty()) return text;
        auto* translator = GlobalEngineContext::getInstance().getTranslatorEngine();
        if (!translator) {
            std::cerr << "Translator not available." << std::endl;
            return text;
        }
        try {
            const size_t MAX_BYTES = 1024;
            std::string input = text;
            if (input.length() > MAX_BYTES) {
                std::cerr << "Warning: Text too long, truncating." << std::endl;
                input = input.substr(0, MAX_BYTES);
            }
            return translator->translate(input, target_lang, max_tokens);
        } catch (...) {
            std::cerr << "Translation error." << std::endl;
            return text;
        }
    }

    // ---------- Markdown 翻译 ----------
    static std::string translate_markdown_content(const std::string& content,
                                                  TranslatorEngine& translator,
                                                  const std::string& target_lang) {
        std::istringstream stream(content);
        std::string line;
        std::ostringstream output;
        bool in_code_block = false;
        bool in_math_block = false;

        while (std::getline(stream, line)) {
            // ---------- 检测 HTML 表格 ----------
            if (line.find("<table>") != std::string::npos) {
                std::string table_html = line + "\n";
                // 继续读取直到遇到 </table>
                while (std::getline(stream, line)) {
                    table_html += line + "\n";
                    if (line.find("</table>") != std::string::npos) {
                        break;
                    }
                }
                // 翻译整个表格
                std::string translated_table = translateHtmlContent(table_html, translator, target_lang);
                output << translated_table << "\n";
                continue;
            }
            if (line.rfind("```", 0) == 0) {
                in_code_block = !in_code_block;
                output << line << "\n";
                continue;
            }
            if (in_code_block) {
                output << line << "\n";
                continue;
            }
            if (line.rfind("$$", 0) == 0 && !in_math_block) {
                in_math_block = true;
                output << line << "\n";
                continue;
            }
            if (in_math_block) {
                output << line << "\n";
                if (line.rfind("$$", 0) == 0) in_math_block = false;
                continue;
            }

            // 标题
            std::regex heading_regex(R"(^(#+)\s+(.+))");
            std::smatch heading_match;
            if (std::regex_search(line, heading_match, heading_regex)) {
                std::string hashes = heading_match[1].str();
                std::string heading_text = heading_match[2].str();
                std::string translated = translate_text(heading_text, target_lang);
                output << hashes << " " << translated << "\n";
                continue;
            }

            // 图片
            std::regex image_regex(R"(!\[([^\]]*)\]\(([^)]*)\))");
            std::smatch image_match;
            if (std::regex_search(line, image_match, image_regex)) {
                std::string alt = image_match[1].str();
                std::string url = image_match[2].str();
                std::string translated_alt = alt.empty() ? alt : translate_text(alt, target_lang);
                output << "![" << translated_alt << "](" << url << ")\n";
                continue;
            }

            // 普通行
            std::cout << "Translating line: " << line << std::endl;
            std::string translated = translate_text(line, target_lang);
            std::cout << "Translated line: " << translated << std::endl;
            output << translated << "\n";
        }
        return output.str();
    }

// ---------- 处理 MD/TXT ----------
    static std::string process_md_txt(const std::string& path, const std::string& target_lang, bool is_md) {
        std::ifstream file(path);
        if (!file.is_open()) throw std::runtime_error("Failed to open file: " + path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        std::string content = buffer.str();

        if (is_md) {
            auto* translator = GlobalEngineContext::getInstance().getTranslatorEngine();
            if (!translator) throw std::runtime_error("Translator not available");
            return translate_markdown_content(content, *translator, target_lang);
        } else {
            // TXT 按行翻译
            std::istringstream stream(content);
            std::string line;
            std::ostringstream output;
            while (std::getline(stream, line)) {
                if (line.empty()) output << "\n";
                else output << translate_text(line, target_lang) << "\n";
            }
            return output.str();
        }
    }

// ---------- 处理 Office 文档 ----------
    static std::string process_office(const std::string& path, const std::string& target_lang,
                                      const std::string& base_dir, const std::string& output_dir) {
        std::string converter_exe = base_dir + "\\office2md.exe";
        if (GetFileAttributesA(converter_exe.c_str()) == INVALID_FILE_ATTRIBUTES)
            throw std::runtime_error("office2md.exe not found at: " + converter_exe);

        // 在输出目录生成临时 .md 文件（不通过 .tmp）
        std::string temp_dir = output_dir.empty() ? "." : output_dir;
        // 创建目录（若不存在）
        std::error_code ec;
        if (!std::filesystem::create_directories(temp_dir, ec) && ec)
            throw std::runtime_error("Failed to create temp directory: " + temp_dir);

        // 使用随机数生成唯一文件名（与 get_temp_md_path 一致）
        static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr) + GetCurrentProcessId()));
        std::uniform_int_distribution<int> dist(1000, 9999);
        std::string filename = "docmd_" + std::to_string(dist(rng)) + "_" + std::to_string(GetCurrentProcessId()) + ".md";
        std::string temp_md = temp_dir + "\\" + filename;
        // 若文件已存在（极低概率），先删除
        std::remove(temp_md.c_str());

        OfficeConverter converter(converter_exe);
        ConversionResult result = converter.convert(path, temp_md);
        if (!result.success) {
            std::remove(temp_md.c_str());
            throw std::runtime_error("Office conversion failed: " + result.error_message);
        }

        // 翻译
        std::string translated;
        try {
            translated = process_md_txt(temp_md, target_lang, true);
        } catch (...) {
            std::remove(temp_md.c_str());
            throw;
        }
        std::remove(temp_md.c_str());
        return translated;
    }
// ---------- 处理图像（文档流程） ----------
    static std::string process_image_doc(const cv::Mat& img, const ProcessorConfig& config,
                                         float threshold, const std::string& target_lang) {
        auto& ctx = GlobalEngineContext::getInstance();
        DocumentProcessor processor(ctx, config);
        auto segments = processor.process(img, threshold);
        std::string md = MarkdownGenerator::generate(segments);
        // 翻译
        if (!md.empty()) {
            // 按段落翻译（简单处理，可改进）
            std::vector<std::string> lines;
            std::istringstream stream(md);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty()) lines.push_back(line);
            }
            std::string translated_md;
            for (const auto& l : lines) {
                if (!l.empty()) {
                    translated_md += translate_text(l, target_lang) + "\n\n";
                }
            }
            if (translated_md.size() >= 2 && translated_md.substr(translated_md.size() - 2) == "\n\n")
                translated_md.resize(translated_md.size() - 2);
            return translated_md;
        }
        return md;
    }

// ---------- FileTranslationModule 实现 ----------

    FileTranslationModule::FileTranslationModule(
            const std::string& target_language,
            bool enable_warp,
            bool enable_enhance,
            float layout_threshold,
            int pdf_dpi)
            : target_language_(target_language.empty() ? ConfigManager::getInstance().getDefaultLanguage() : target_language),
              enable_warp_(enable_warp),
              enable_enhance_(enable_enhance),
              layout_threshold_(layout_threshold),
              pdf_dpi_(pdf_dpi) {}


    std::string FileTranslationModule::process(const std::string& input_path, const std::string& output_path) {
        // 初始化全局上下文
        auto& ctx = GlobalEngineContext::getInstance();
        ctx.initialize(); // 使用默认 base_dir（exe目录）

        // 获取扩展名
        std::string ext;
        size_t dot = input_path.find_last_of('.');
        if (dot != std::string::npos) {
            ext = input_path.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        } else {
            throw std::runtime_error("No file extension.");
        }

        // 确定最终输出路径
        std::string final_output = output_path;
        if (final_output.empty()) {
            final_output = auto_output_path(input_path, ext);
        }

        // 在确定 final_output 之后，提取输出目录
        std::string output_dir;
        size_t slash = final_output.find_last_of("\\/");
        if (slash != std::string::npos) {
            output_dir = final_output.substr(0, slash + 1);  // 包含末尾分隔符
        } else {
            output_dir = "./";  // 当前目录
        }

        std::string result_content;

        // 根据类型处理
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" || ext == "tiff") {
            // 文档图片流程
            cv::Mat img = imread_unicode(input_path);
            if (img.empty()) throw std::runtime_error("Failed to load image: " + input_path);

            ProcessorConfig config;
            config.assets_dir = output_dir + "assets";   // 动态设置
            config.enable_warp = enable_warp_;
            config.enable_enhance = enable_enhance_;
            result_content = process_image_doc(img, config, layout_threshold_, target_language_);
        }
        else if (ext == "pdf") {
            auto pages = pdf_to_images(input_path, pdf_dpi_);
            std::vector<std::string> all_segments;
            ProcessorConfig config;
            config.assets_dir = output_dir + "assets";   // 动态设置
            config.enable_warp = false;   // PDF 不启用 warp
            config.enable_enhance = false;
            for (const auto& page : pages) {
                auto md = process_image_doc(page, config, layout_threshold_, target_language_);
                if (!md.empty()) all_segments.push_back(md);
            }
            result_content = MarkdownGenerator::generate(all_segments);
        }
        else if (ext == "md") {
            result_content = process_md_txt(input_path, target_language_, true);
        }
        else if (ext == "txt") {
            result_content = process_md_txt(input_path, target_language_, false);
        }
        else if (ext == "docx" || ext == "pptx" || ext == "xlsx") {
            std::string base = get_exe_directory();
            std::string output_dir;
            size_t slash = final_output.find_last_of("\\/");
            if (slash != std::string::npos) {
                output_dir = final_output.substr(0, slash);
            }
            result_content = process_office(input_path, target_language_, base, output_dir);
        }
        else {
            throw std::runtime_error("Unsupported file type: " + ext);
        }

        // 写入文件
        std::ofstream out(final_output);
        if (!out) throw std::runtime_error("Failed to write output file: " + final_output);
        out << result_content;
        out.close();

        return final_output;
    }

} // namespace docmind