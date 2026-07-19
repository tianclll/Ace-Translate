#include "docmind/utils/ImageLayout.hpp"
#include <algorithm>
#include <cmath>

namespace docmind {

    ImageLayout::ImageLayout(FontManager* fontMgr, float scaleFactor)
            : fontMgr_(fontMgr), scaleFactor_(scaleFactor) {}

    ImageLayout::~ImageLayout() = default;

    cv::Rect ImageLayout::getBoundingRect(const std::vector<cv::Point2f>& box) const {
        if (box.size() < 4) return cv::Rect();
        return cv::boundingRect(box);
    }

    FontInfo ImageLayout::computeFont(const std::vector<cv::Point2f>& box,
                                      const std::string& text,
                                      const cv::Size& imageSize) {
        FontInfo info;
        if (box.size() < 4) return info;

        cv::Rect rect = getBoundingRect(box);
        float scale = scaleFactor_;
        int cx = rect.x + rect.width / 2;
        int cy = rect.y + rect.height / 2;
        int newW = std::max(1, static_cast<int>(rect.width * scale));
        int newH = std::max(1, static_cast<int>(rect.height * scale));
        rect.x = cx - newW / 2;
        rect.y = cy - newH / 2;
        rect.width = newW;
        rect.height = newH;
        rect.x = std::max(0, rect.x);
        rect.y = std::max(0, rect.y);
        rect.width = std::min(rect.width, imageSize.width - rect.x);
        rect.height = std::min(rect.height, imageSize.height - rect.y);

        int margin = 2;
        int maxWidth = std::max(1, rect.width - 2 * margin);
        int maxHeight = std::max(1, rect.height - 2 * margin);
        info.maxWidth = maxWidth;
        info.maxHeight = maxHeight;
        info.fontPath = fontMgr_ ? fontMgr_->getFontPath() : "";
        info.fontSize = 1;   // 绘制时动态计算
        info.lineSpacing = 0;
        info.position = cv::Point(rect.x + margin, rect.y + margin);
        info.position.x = std::max(0, std::min(info.position.x, imageSize.width - 1));
        info.position.y = std::max(0, std::min(info.position.y, imageSize.height - 1));

        return info;
    }

} // namespace docmind