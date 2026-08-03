#include "docmind/engines/TranslatorEngine.hpp"
#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <sstream>

namespace docmind {

    // 用 SEH 保护的 translator_create 调用（防止因 llama 加载模型崩溃或死锁）
    using TranslatorCreateFunc = void* (*)(const char*, int);
    static void* safe_translator_create(TranslatorCreateFunc func, const char* path, int n_gpu_layers) {
        volatile void* result = nullptr;
        volatile bool done = false;
        std::thread t([&]() {
            __try {
                const_cast<volatile void*&>(result) = func(path, n_gpu_layers);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                result = nullptr;
            }
            const_cast<volatile bool&>(done) = true;
        });
        t.detach();
        // 等待最多 30 秒
        auto start = std::chrono::steady_clock::now();
        while (!done) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
                std::cerr << "translator_create timed out after 30s" << std::endl;
                std::cerr.flush();
                // 无法强制终止线程，只能返回 nullptr
                break;
            }
            Sleep(100);
        }
        return const_cast<void*>(result);
    }

    TranslatorEngine::TranslatorEngine(DLLLoader& loader,
                                       const std::string& model_path,
                                       const std::string& target_language,
                                       int n_gpu_layers)
            : dll_(loader), default_lang_(target_language.empty() ? "English" : target_language) {
        if (!dll_.translatorLoaded()) {
            throw std::runtime_error("Translator DLL not loaded");
        }
        if (!dll_.translator_create || !dll_.translator_destroy ||
            !dll_.translator_translate || !dll_.translator_free_string) {
            throw std::runtime_error("Translator function pointers missing");
        }
        handle_ = safe_translator_create(dll_.translator_create, model_path.c_str(), n_gpu_layers);
        if (!handle_) {
            std::cerr << "translator_create SEH crashed for model: " << model_path << std::endl;
            std::cerr.flush();
            throw std::runtime_error("translator_create SEH crashed: " + model_path);
        }
        initialized_ = true;
        std::cout << "TranslatorEngine initialized. Default language: " << default_lang_ << std::endl;
    }

    TranslatorEngine::~TranslatorEngine() {
        if (handle_ && dll_.translator_destroy) {
            dll_.translator_destroy(handle_);
            handle_ = nullptr;
        }
    }

    std::string TranslatorEngine::translate(const std::string& text, int max_tokens) {
        return translate(text, default_lang_, max_tokens);
    }

    std::string TranslatorEngine::translate(const std::string& text, const std::string& target_language, int max_tokens) {
        if (!initialized_ || text.empty() || target_language.empty()) {
            return text;
        }

        // 如果有术语，拼到 prompt 前缀
        std::string full_text = text;
        if (!glossary_.empty()) {
            std::string prompt;
            prompt += "参考下面的翻译：\n";
            for (const auto& [term, trans] : glossary_) {
                prompt += term + " 翻译成 " + trans + "\n";
            }
            prompt += "\n将以下文本翻译为" + target_language + "。\n\n";
            prompt += "要求：\n";
            prompt += "1. 只输出翻译结果，不输出任何其他内容\n";
            prompt += "2. 上面列出的术语必须使用对应译文翻译\n";
            prompt += "3. 禁止添加任何解释、备注、拼音、英文原文或序号\n\n";
            full_text = prompt + text;
        }

        // 限制 prompt 总长度（防止术语过多导致 DLL 崩溃）
        constexpr size_t MAX_PROMPT_BYTES = 4096;
        if (full_text.length() > MAX_PROMPT_BYTES) {
            full_text = full_text.substr(0, MAX_PROMPT_BYTES);
        }

        const char* result = dll_.translator_translate(
                handle_,
                full_text.c_str(),
                target_language.c_str(),
                max_tokens
        );
        std::string translated;
        if (result) {
            translated = result;
            dll_.translator_free_string(result);
        } else {
            std::cerr << "Translation failed for: " << text.substr(0, 60) << std::endl;
            translated = text;
        }

        // 后处理：过滤掉 LLM 可能添加的备注/解释/编号等噪音行
        if (!translated.empty() && !glossary_.empty()) {
            std::string cleaned;
            std::istringstream stream(translated);
            std::string line;
            while (std::getline(stream, line)) {
                std::string trimmed;
                // 去掉首尾空白
                size_t s = line.find_first_not_of(" \t\r\n");
                size_t e = line.find_last_not_of(" \t\r\n");
                if (s == std::string::npos) { cleaned += "\n"; continue; }
                trimmed = line.substr(s, e - s + 1);
                if (trimmed.empty()) { cleaned += "\n"; continue; }

                bool skip = false;

                // 跳过常见备注关键词（中文 UTF-8 编码）
                if (!skip) {
                    static const std::string note_cn[] = {
                        "\xe5\xa4\x87\xe6\xb3\xa8",    // 备注
                        "\xe6\xb3\xa8\xe6\x84\x8f",    // 注意
                        "\xe6\xb3\xa8\xe9\x87\x8a",    // 注释
                    };
                    static const std::string note_en[] = {
                        "Note:", "NOTE:", "note:"
                    };
                    for (const auto& n : note_cn) {
                        if (trimmed.size() >= n.size() && trimmed.substr(0, n.size()) == n) { skip = true; break; }
                    }
                    if (!skip) {
                        for (const auto& n : note_en) {
                            if (trimmed.size() >= n.size() && trimmed.substr(0, n.size()) == n) { skip = true; break; }
                        }
                    }
                }

                // 跳过编号行（如 "1. ", "2."）
                if (!skip && !trimmed.empty() && trimmed[0] >= '1' && trimmed[0] <= '9') {
                    if (trimmed.size() >= 2 && trimmed[1] == '.') skip = true;
                }

                // 跳过包含原始术语但无翻译的噪音行（如 "Attention（英语词汇..."）
                if (!skip) {
                    for (const auto& [term, trans] : glossary_) {
                        if (!term.empty() && !trans.empty()) {
                            std::string marker = term + "\xef\xbc\x88";  // term + "（"
                            if (trimmed.find(marker) != std::string::npos) {
                                skip = true; break;
                            }
                        }
                    }
                }

                if (!skip) cleaned += line + "\n";
            }
            // 去掉末尾多余空行
            while (!cleaned.empty() && (cleaned.back() == '\n' || cleaned.back() == '\r'))
                cleaned.pop_back();
            if (!cleaned.empty()) translated = cleaned;
        }

        return translated;
    }

    void TranslatorEngine::setGlossaryTerms(const std::vector<std::pair<std::string, std::string>>& terms) {
        clearGlossary();
        glossary_ = terms;
    }

    void TranslatorEngine::clearGlossary() {
        glossary_.clear();
    }

    std::string TranslatorEngine::summarize(const std::string& text, int max_tokens) {
        if (!initialized_ || text.empty()) {
            return text;
        }
        // 直接调用 DLL（与 translate() 相同的调用方式，没有 SEH 线程包装）
        const char* result = dll_.translator_summarize(
                handle_, text.c_str(), max_tokens);
        std::string summary;
        if (result) {
            summary = result;
            dll_.translator_free_string(const_cast<const char*>(result));
        } else {
            std::cerr << "Summarize failed for: " << text.substr(0, 60) << std::endl;
            summary = text;
        }
        return summary;
    }

} // namespace docmind