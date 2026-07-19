#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

struct LayoutBox
{
    int class_id;
    std::string label;
    float score;
    cv::Rect box;
    int read_order = -1;   // 最终排序序号（从1开始）
};

class PPDocLayoutV2 {
public:
    bool Init(const std::string& model_path, bool use_gpu = true, int device_id = 0);

    // score_threshold: 全局阈值（>0 时覆盖类别默认阈值）
    std::vector<LayoutBox> Detect(
            const cv::Mat& image,
            float score_threshold = 0.5f
    );

private:
    // 辅助结构（内部使用）
    struct Detection {
        int class_id;
        float score;
        float x1, y1, x2, y2;
        float order_major;   // p[6]
        float order_minor;   // p[7]
    };

    // 预处理：resize 至 800x800，归一化到 [0,1]，CHW 排列
    std::vector<float> Preprocess(
            const cv::Mat& image,
            std::vector<float>& im_shape,
            std::vector<float>& scale_factor
    );

    // 后处理：阈值过滤、NMS、大图过滤、排序、构建结果
    std::vector<LayoutBox> PostProcess(
            const std::vector<Detection>& detections,
            const cv::Size& image_size,
            float global_threshold
    );

    // 计算 IoU
    static float ComputeIoU(const Detection& a, const Detection& b);

    // NMS（按分数降序，同类用 iou_same，异类用 iou_diff）
    static std::vector<int> NonMaxSuppression(
            const std::vector<Detection>& boxes,
            float iou_same = 0.6f,
            float iou_diff = 0.98f
    );

private:
    static constexpr int INPUT_SIZE = 800;

    const std::vector<std::string> LABELS = {
            "abstract",           // 0
            "algorithm",          // 1
            "aside_text",         // 2
            "chart",              // 3
            "content",            // 4
            "formula",            // 5
            "doc_title",          // 6
            "figure_title",       // 7
            "footer",             // 8
            "footer_image",       // 9
            "footnote",           // 10
            "formula_number",     // 11
            "header",             // 12
            "header_image",       // 13
            "image",              // 14
            "inline_formula",     // 15
            "number",             // 16
            "paragraph_title",    // 17
            "reference",          // 18
            "reference_content",  // 19
            "seal",               // 20
            "table",              // 21
            "text",               // 22
            "vertical_text",      // 23
            "vision_footnote"     // 24
    };

    const std::unordered_map<int, float> SCORE_THRESHOLDS = {
            {0, 0.5f},   // abstract
            {1, 0.5f},   // algorithm
            {2, 0.5f},   // aside_text
            {3, 0.5f},   // chart
            {4, 0.5f},   // content
            {5, 0.5f},   // formula
            {6, 0.4f},   // doc_title
            {7, 0.5f},   // figure_title
            {8, 0.5f},   // footer
            {9, 0.5f},   // footer_image
            {10, 0.5f},  // footnote
            {11, 0.5f},  // formula_number
            {12, 0.5f},  // header
            {13, 0.5f},  // header_image
            {14, 0.5f},  // image
            {15, 0.5f},  // inline_formula
            {16, 0.5f},  // number
            {17, 0.4f},  // paragraph_title
            {18, 0.5f},  // reference
            {19, 0.5f},  // reference_content
            {20, 0.45f}, // seal
            {21, 0.5f},  // table
            {22, 0.4f},  // text
            {23, 0.4f},  // vertical_text
            {24, 0.5f},  // vision_footnote
    };

    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "PPDocLayoutV2"};
    Ort::AllocatorWithDefaultOptions allocator_;
    std::unique_ptr<Ort::Session> session_;
};