#include "docmind/modules/TextTranslationModule.hpp"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/core/ConfigManager.hpp"
#include <stdexcept>

namespace docmind {

    TextTranslationModule::TextTranslationModule(const std::string& target_language)
            : target_language_(target_language.empty() ? ConfigManager::getInstance().getDefaultLanguage() : target_language) {
        // 确保全局上下文已初始化
        GlobalEngineContext::getInstance().initialize();
    }

    std::string TextTranslationModule::translate(const std::string& text, int max_tokens) {
        if (text.empty()) return text;

        auto* translator = GlobalEngineContext::getInstance().getTranslatorEngine();
        if (!translator) {
            throw std::runtime_error("Translator engine not available");
        }

        return translator->translate(text, target_language_, max_tokens);
    }

} // namespace docmind