#include "docmind/modules/TextTranslationModule.hpp"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/core/ConfigManager.hpp"
#include <stdexcept>
#include <sstream>

namespace docmind {

    TextTranslationModule::TextTranslationModule(const std::string& target_language)
            : target_language_(target_language.empty() ? ConfigManager::getInstance().getDefaultLanguage() : target_language) {
        // 确保全局上下文已初始化
        GlobalEngineContext::getInstance().initialize();
    }

    std::string TextTranslationModule::translate(const std::string& text, int max_tokens) {
        if (text.empty()) return text;

        // 确保翻译引擎已加载（支持懒加载）
        GlobalEngineContext::getInstance().ensureTranslatorEngine();

        auto* translator = GlobalEngineContext::getInstance().getTranslatorEngine();
        if (!translator) {
            throw std::runtime_error("Translator engine not available");
        }

        // 按空行分段，逐段翻译后拼接（避免引擎内的 \n\n 截断逻辑）
        std::vector<std::string> paragraphs;
        std::istringstream stream(text);
        std::string line;
        std::string current;
        while (std::getline(stream, line)) {
            if (line.empty()) {
                if (!current.empty()) {
                    paragraphs.push_back(current);
                    current.clear();
                }
            } else {
                if (!current.empty()) current += "\n";
                current += line;
            }
        }
        if (!current.empty()) paragraphs.push_back(current);

        std::string result;
        for (size_t i = 0; i < paragraphs.size(); ++i) {
            if (!result.empty()) result += "\n\n";
            result += translator->translate(paragraphs[i], target_language_, max_tokens);
        }

        return result;
    }

} // namespace docmind