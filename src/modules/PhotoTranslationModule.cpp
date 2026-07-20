#include "docmind/modules/PhotoTranslationModule.hpp"
#include "docmind/utils/ImageUtils.hpp"
#include "docmind/core/ConfigManager.hpp"
#include <vector>
#include <iostream>

namespace docmind {

    PhotoTranslationModule::PhotoTranslationModule(
            const std::string& target_language,
            const std::string& font_path,
            float scale_factor)
            : ctx_(GlobalEngineContext::getInstance()),
              target_language_(target_language.empty() ? ConfigManager::getInstance().getDefaultLanguage() : target_language),
              scale_factor_(scale_factor) {

        ctx_.initialize(); // 确保引擎已初始化
        font_manager_ = std::make_unique<FontManager>(font_path);
        layout_ = std::make_unique<ImageLayout>(font_manager_.get(), scale_factor_);
        renderer_ = std::make_unique<ImageRenderer>(font_manager_.get());
    }

    cv::Mat PhotoTranslationModule::process(const cv::Mat& input, int max_tokens) {
        if (input.empty()) return {};

        // 确保引擎已加载（支持懒加载）
        ctx_.ensureOCREngine();
        ctx_.ensureTranslatorEngine();

        auto* ocr = ctx_.getOCREngine();
        auto* translator = ctx_.getTranslatorEngine();
        if (!ocr || !translator) {
            std::cerr << "OCR or Translator engine not available." << std::endl;
            return input.clone();
        }

        // 1. OCR 识别
        auto ocr_results = ocr->recognizeBuffer(input);
        if (ocr_results.empty()) {
            std::cout << "No text detected." << std::endl;
            return input.clone();
        }

        // 2. 翻译并准备块
        struct Block {
            std::vector<cv::Point2f> box;
            std::string translated_text;
        };
        std::vector<Block> blocks;
        blocks.reserve(ocr_results.size());

        for (const auto& res : ocr_results) {
            if (res.text.empty()) continue;
            // 将 OCRResult 的 box (vector<vector<float>>) 转为 vector<cv::Point2f>
            std::vector<cv::Point2f> points;
            for (const auto& pt : res.box) {
                if (pt.size() >= 2) {
                    points.emplace_back(pt[0], pt[1]);
                }
            }
            if (points.size() < 4) continue; // 跳过无效框

            std::string translated = translator->translate(res.text, target_language_, max_tokens);
            if (!translated.empty()) {
                blocks.push_back({points, translated});
            }
        }

        if (blocks.empty()) {
            return input.clone();
        }

        // 3. 渲染翻译结果到图像
        cv::Mat output = input.clone();
        for (auto& block : blocks) {
            auto font_info = layout_->computeFont(block.box, block.translated_text, output.size());
            cv::Rect drawRect(font_info.position.x, font_info.position.y,
                              font_info.maxWidth, font_info.maxHeight);

            cv::Scalar originalColor = PhotoUtils::getTextColor(input, block.box);
            double brightness = 0.299 * originalColor[2] + 0.587 * originalColor[1] + 0.114 * originalColor[0];
            cv::Scalar finalColor = (brightness > 128) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);
            renderer_->setTextColor(finalColor);

            renderer_->eraseRect(output, drawRect);
            renderer_->drawText(output, block.translated_text, font_info);
        }

        return output;
    }

} // namespace docmind
