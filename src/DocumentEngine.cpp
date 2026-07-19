#include "docmind/DocumentEngine.h"
#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/modules/FileTranslationModule.hpp"
#include "docmind/modules/PhotoTranslationModule.hpp"
#include "docmind/modules/TextTranslationModule.hpp"
#include "docmind/modules/ScreenshotTranslationModule.hpp"
#include <fstream>
#include <filesystem>


std::string process_image(
        const std::string& image_path,
        const std::string& base_dir,
        const std::string& target_language,
        float layout_threshold
) {
    docmind::FileTranslationModule module(target_language, true, false, layout_threshold);
    return module.process(image_path, "");
}

std::string process_pdf(
        const std::string& pdf_path,
        const std::string& base_dir,
        const std::string& target_language,
        float layout_threshold,
        int dpi
) {
    docmind::FileTranslationModule module(target_language, false, false, layout_threshold, dpi);
    return module.process(pdf_path, "");
}


std::string process_photo(
        const std::string& image_path,
        const std::string& output_path,
        const std::string& base_dir,
        const std::string& target_language,
        int max_tokens) {

    auto& ctx = docmind::GlobalEngineContext::getInstance();
    ctx.initialize(base_dir);

    cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);
    if (img.empty()) {
        throw std::runtime_error("Failed to load image: " + image_path);
    }

    std::string final_output = output_path;
    if (final_output.empty()) {
        std::filesystem::path p(image_path);
        std::string stem = p.stem().string();
        std::string ext = p.extension().string();
        final_output = p.parent_path().string() + "/" + stem + "_trans" + ext;
    }

    std::filesystem::create_directories(std::filesystem::path(final_output).parent_path());

    docmind::PhotoTranslationModule module(target_language, "", 1.2f);
    cv::Mat result = module.process(img, max_tokens);

    if (result.empty()) {
        throw std::runtime_error("图片翻译引擎返回空结果");
    }

    if (!cv::imwrite(final_output, result)) {
        throw std::runtime_error("Failed to write output image: " + final_output);
    }

    return final_output;
}

std::string translate_text(const std::string& text, const std::string& target_language, int max_tokens) {
    docmind::TextTranslationModule module(target_language);
    return module.translate(text, max_tokens);
}

std::string translate_screenshot_image(const cv::Mat& image, const std::string& target_language, int max_tokens) {
    docmind::ScreenshotTranslationModule module(target_language);
    return module.translate(image, max_tokens);
}
