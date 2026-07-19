#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "FontManager.hpp"

namespace docmind {

    struct FontInfo {
        std::string fontPath;
        int fontSize;
        cv::Point position;
        int lineSpacing;
        int maxWidth;
        int maxHeight;
    };

    class ImageLayout {
    public:
        explicit ImageLayout(FontManager* fontMgr, float scaleFactor = 1.2f);
        ~ImageLayout();

        FontInfo computeFont(const std::vector<cv::Point2f>& box,
                             const std::string& text,
                             const cv::Size& imageSize);

        void setScaleFactor(float factor) { scaleFactor_ = factor; }

    private:
        FontManager* fontMgr_;
        float scaleFactor_;
        cv::Rect getBoundingRect(const std::vector<cv::Point2f>& box) const;
    };

} // namespace docmind