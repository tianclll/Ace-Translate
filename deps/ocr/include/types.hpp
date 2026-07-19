// include/types.hpp
#ifndef TYPES_HPP
#define TYPES_HPP

#include <vector>
#include <string>
#include "export.hpp"

namespace ocr {

    struct OCR_API OCRResult {
        std::vector<std::vector<float>> box;  // 4个点 [x1,y1,x2,y2,x3,y3,x4,y4]
        std::string text;
        float score;
    };

} // namespace ocr

#endif // TYPES_HPP