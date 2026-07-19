#include "docmind/modules/ScreenshotTranslationModule.hpp"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/core/ConfigManager.hpp"
#include <stdexcept>

namespace docmind {

    ScreenshotTranslationModule::ScreenshotTranslationModule(const std::string& target_language)
            : target_language_(target_language.empty() ? ConfigManager::getInstance().getDefaultLanguage() : target_language) {
        GlobalEngineContext::getInstance().initialize();
    }

    std::string ScreenshotTranslationModule::translate(const cv::Mat& image, int max_tokens) {
        if (image.empty()) return "";

        auto* ocr = GlobalEngineContext::getInstance().getOCREngine();
        auto* translator = GlobalEngineContext::getInstance().getTranslatorEngine();
        if (!ocr || !translator) {
            throw std::runtime_error("OCR or Translator engine not available");
        }

        // OCR 识别
        auto ocr_results = ocr->recognizeBuffer(image);
        if (ocr_results.empty()) return "";

        // 拼接所有识别文本（用空格分隔）
        std::string combined_text;
        for (const auto& res : ocr_results) {
            if (!res.text.empty()) {
                if (!combined_text.empty()) combined_text += " ";
                combined_text += res.text;
            }
        }

        if (combined_text.empty()) return "";
        // 翻译
        return translator->translate(combined_text, target_language_, max_tokens);
    }

} // namespace docmind