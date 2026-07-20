#include "docmind/engines/TranslatorEngine.hpp"
#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>

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
        const char* result = dll_.translator_translate(
                handle_,
                text.c_str(),
                target_language.c_str(),
                max_tokens
        );
        std::string translated;
        if (result) {
            translated = result;
            dll_.translator_free_string(result);
        } else {
            std::cerr << "Translation failed for: " << text << std::endl;
            translated = text;
        }
        return translated;
    }

} // namespace docmind