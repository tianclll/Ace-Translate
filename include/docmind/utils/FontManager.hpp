#pragma once
#include <opencv2/opencv.hpp>
#include <string>

namespace docmind {

    class FontManager {
    public:
        explicit FontManager(const std::string& fontPath);
        ~FontManager();

        cv::Size getTextSize(const std::string& text, int fontSize) const;
        cv::Size getTextSize(const std::string& text, int fontSize, int* baseline) const;

        std::string getFontPath() const;
        void setFontPath(const std::string& path);

    private:
        std::string fontPath_;
    };

} // namespace docmind