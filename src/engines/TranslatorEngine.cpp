#include "docmind/engines/TranslatorEngine.hpp"
#include <iostream>

namespace docmind {

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
        handle_ = dll_.translator_create(model_path.c_str(), n_gpu_layers);
        if (!handle_) {
            throw std::runtime_error("Failed to create translator instance with model: " + model_path);
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