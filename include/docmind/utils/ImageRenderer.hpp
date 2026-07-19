#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include "ImageLayout.hpp"

namespace docmind {

    class FontManager;

    class ImageRenderer {
    public:
        explicit ImageRenderer(FontManager* fontMgr);
        ~ImageRenderer();

        void eraseText(cv::Mat& img, const std::vector<cv::Point2f>& box);
        void eraseRect(cv::Mat& img, const cv::Rect& rect);
        void drawText(cv::Mat& img, const std::string& text, const FontInfo& info);

        void setTextColor(const cv::Scalar& color);
        void setBackgroundColor(const cv::Scalar& color);

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace docmind