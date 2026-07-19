#include "docmind/utils/FontManager.hpp"
#include <opencv2/opencv.hpp>

namespace docmind {

    FontManager::FontManager(const std::string& fontPath) : fontPath_(fontPath) {}
    FontManager::~FontManager() = default;

    cv::Size FontManager::getTextSize(const std::string& text, int fontSize) const {
        int baseline;
        return getTextSize(text, fontSize, &baseline);
    }

    cv::Size FontManager::getTextSize(const std::string& text, int fontSize, int* baseline) const {
        int baseline_tmp;
        cv::Size sz = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontSize / 10.0, 1, &baseline_tmp);
        if (sz.width == 0 && sz.height == 0 && !text.empty()) {
            float charWidth = fontSize * 3.0f;
            float charHeight = fontSize * 1.0f;
            int len = static_cast<int>(text.length());
            sz.width = static_cast<int>(len * charWidth);
            sz.height = static_cast<int>(charHeight);
            baseline_tmp = static_cast<int>(fontSize * 1.2f);
        }
        if (baseline) *baseline = baseline_tmp;
        return sz;
    }

    std::string FontManager::getFontPath() const { return fontPath_; }
    void FontManager::setFontPath(const std::string& path) { fontPath_ = path; }

} // namespace docmind