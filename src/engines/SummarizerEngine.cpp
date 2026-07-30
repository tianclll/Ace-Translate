#include "docmind/engines/SummarizerEngine.hpp"
#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>

namespace docmind {

    // 用 SEH 保护的 summarizer_create 调用
    using SummarizerCreateFunc = void* (*)(const char*, int);
    static void* safe_summarizer_create(SummarizerCreateFunc func, const char* path, int n_gpu_layers) {
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
        auto start = std::chrono::steady_clock::now();
        while (!done) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
                std::cerr << "summarizer_create timed out after 30s" << std::endl;
                std::cerr.flush();
                break;
            }
            Sleep(100);
        }
        return const_cast<void*>(result);
    }

    SummarizerEngine::SummarizerEngine(DLLLoader& loader,
                                       const std::string& model_path,
                                       int n_gpu_layers)
            : dll_(loader) {
        if (!dll_.summarizerLoaded()) {
            throw std::runtime_error("Summarizer DLL not loaded");
        }
        if (!dll_.summarizer_create || !dll_.summarizer_destroy ||
            !dll_.summarizer_summarize || !dll_.summarizer_free_string) {
            throw std::runtime_error("Summarizer function pointers missing");
        }
        handle_ = safe_summarizer_create(dll_.summarizer_create, model_path.c_str(), n_gpu_layers);
        if (!handle_) {
            std::cerr << "summarizer_create SEH crashed for model: " << model_path << std::endl;
            std::cerr.flush();
            throw std::runtime_error("summarizer_create SEH crashed: " + model_path);
        }
        initialized_ = true;
        std::cout << "SummarizerEngine initialized." << std::endl;
    }

    SummarizerEngine::~SummarizerEngine() {
        if (handle_ && dll_.summarizer_destroy) {
            dll_.summarizer_destroy(handle_);
            handle_ = nullptr;
        }
    }

    std::string SummarizerEngine::summarize(const std::string& text, int max_tokens) {
        if (!initialized_ || text.empty()) {
            return text;
        }
        const char* result = dll_.summarizer_summarize(
                handle_, text.c_str(), max_tokens);
        std::string summary;
        if (result) {
            summary = result;
            dll_.summarizer_free_string(result);
        } else {
            std::cerr << "Summarize failed for: " << text.substr(0, 60) << std::endl;
            summary = text;
        }
        return summary;
    }

} // namespace docmind
