#pragma once

#include "DLLLoader.hpp"
#include <string>

namespace docmind {

    class TranslatorEngine {
    public:
        TranslatorEngine(DLLLoader& loader,
                         const std::string& model_path,
                         const std::string& target_language,
                         int n_gpu_layers = 99);
        ~TranslatorEngine();

        bool isLoaded() const { return handle_ != nullptr; }

        // 使用默认语言翻译
        std::string translate(const std::string& text, int max_tokens = 512);

        // 指定目标语言翻译
        std::string translate(const std::string& text, const std::string& target_language, int max_tokens = 512);

        void setDefaultLanguage(const std::string& lang) { default_lang_ = lang; }

    private:
        DLLLoader& dll_;
        void* handle_ = nullptr;
        std::string default_lang_;  // 默认语言
        bool initialized_ = false;
    };

} // namespace docmind