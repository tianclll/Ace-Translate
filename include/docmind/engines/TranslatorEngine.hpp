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

        // 生成中文摘要
        std::string summarize(const std::string& text, int max_tokens = 256);

        void setDefaultLanguage(const std::string& lang) { default_lang_ = lang; }

        /// 设置当前翻译用的术语表（原文→译文），翻译时自动注入 prompt
        void setGlossaryTerms(const std::vector<std::pair<std::string, std::string>>& terms);
        void clearGlossary();

    private:
        DLLLoader& dll_;
        void* handle_ = nullptr;
        std::string default_lang_;  // 默认语言
        bool initialized_ = false;
        std::vector<std::pair<std::string, std::string>> glossary_;  // 术语表（原文→译文）
    };

} // namespace docmind