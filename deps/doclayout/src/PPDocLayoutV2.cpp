#include "PPDocLayoutV2.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

// ------------------------------------------------------------
// 预处理（与 Python 完全一致）
// ------------------------------------------------------------
std::vector<float> PPDocLayoutV2::Preprocess(
        const cv::Mat& src,
        std::vector<float>& im_shape,
        std::vector<float>& scale_factor)
{
    const int resize_h = INPUT_SIZE;
    const int resize_w = INPUT_SIZE;

    int ori_h = src.rows;
    int ori_w = src.cols;

    cv::Mat img;
    cv::resize(src, img, cv::Size(resize_w, resize_h), 0, 0, cv::INTER_CUBIC);

    im_shape = { static_cast<float>(resize_h), static_cast<float>(resize_w) };
    scale_factor = {
            static_cast<float>(resize_h) / ori_h,
            static_cast<float>(resize_w) / ori_w
    };

    // 归一化到 [0,1]
    img.convertTo(img, CV_32FC3, 1.0 / 255.0);

    std::vector<float> input(3 * resize_h * resize_w);
    int hw = resize_h * resize_w;

    for (int y = 0; y < resize_h; ++y) {
        const auto* row = img.ptr<cv::Vec3f>(y);
        for (int x = 0; x < resize_w; ++x) {
            auto p = row[x];
            input[y * resize_w + x] = p[0];                 // R
            input[hw + y * resize_w + x] = p[1];            // G
            input[2 * hw + y * resize_w + x] = p[2];        // B
        }
    }
    return input;
}

// ------------------------------------------------------------
// IoU 计算
// ------------------------------------------------------------
float PPDocLayoutV2::ComputeIoU(const Detection& a, const Detection& b)
{
    float x1 = std::max(a.x1, b.x1);
    float y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2);
    float y2 = std::min(a.y2, b.y2);

    float inter_w = std::max(0.0f, x2 - x1);
    float inter_h = std::max(0.0f, y2 - y1);
    float inter_area = inter_w * inter_h;

    float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
    float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
    float union_area = area_a + area_b - inter_area;

    if (union_area <= 0.0f) return 0.0f;
    return inter_area / union_area;
}

// ------------------------------------------------------------
// NMS（与 Python 逻辑一致）
// ------------------------------------------------------------
std::vector<int> PPDocLayoutV2::NonMaxSuppression(
        const std::vector<Detection>& boxes,
        float iou_same,
        float iou_diff)
{
    if (boxes.empty()) return {};

    // 按分数降序排列索引
    std::vector<int> indices(boxes.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&](int i, int j) { return boxes[i].score > boxes[j].score; });

    std::vector<int> keep;
    while (!indices.empty()) {
        int current = indices.front();
        keep.push_back(current);
        indices.erase(indices.begin());

        std::vector<int> remaining;
        for (int idx : indices) {
            float iou = ComputeIoU(boxes[current], boxes[idx]);
            float thresh = (boxes[current].class_id == boxes[idx].class_id) ? iou_same : iou_diff;
            if (iou < thresh) {
                remaining.push_back(idx);
            }
        }
        indices = std::move(remaining);
    }
    return keep;
}

// ------------------------------------------------------------
// 后处理主函数
// ------------------------------------------------------------
std::vector<LayoutBox> PPDocLayoutV2::PostProcess(
        const std::vector<Detection>& detections,
        const cv::Size& image_size,
        float global_threshold)
{
    std::vector<LayoutBox> results;

    // 1. 阈值过滤 & 坐标取整
    std::vector<Detection> filtered;
    filtered.reserve(detections.size());

    for (const auto& d : detections) {
        // 类别合法性检查
        if (d.class_id < 0 || d.class_id >= static_cast<int>(LABELS.size()))
            continue;

        // 确定阈值：若 global_threshold > 0 则使用，否则用类别默认
        float th = global_threshold;
        if (th <= 0.0f) {
            auto it = SCORE_THRESHOLDS.find(d.class_id);
            th = (it != SCORE_THRESHOLDS.end()) ? it->second : 0.5f;
        }

        if (d.score < th)
            continue;

        // 取整坐标（Python 中 round 后转 int）
        Detection rounded = d;
        rounded.x1 = std::round(d.x1);
        rounded.y1 = std::round(d.y1);
        rounded.x2 = std::round(d.x2);
        rounded.y2 = std::round(d.y2);

        // 最小尺寸过滤（宽高 < 6 像素）
        float w = rounded.x2 - rounded.x1;
        float h = rounded.y2 - rounded.y1;
        if (w < 6.0f || h < 6.0f)
            continue;

        filtered.push_back(rounded);
    }

    if (filtered.empty())
        return results;

    // 2. NMS
    auto keep_indices = NonMaxSuppression(filtered, 0.6f, 0.98f);
    std::vector<Detection> nms_boxes;
    nms_boxes.reserve(keep_indices.size());
    for (int idx : keep_indices) {
        nms_boxes.push_back(filtered[idx]);
    }

    // 3. 大图过滤（针对 “image” 类别）
    const int image_label_idx = 14; // LABELS 中的 "image"
    float img_area = static_cast<float>(image_size.width * image_size.height);
    float area_thres = (image_size.width > image_size.height) ? 0.82f : 0.93f;

    std::vector<Detection> after_large_filter;
    after_large_filter.reserve(nms_boxes.size());
    for (const auto& d : nms_boxes) {
        if (d.class_id == image_label_idx) {
            float box_area = (d.x2 - d.x1) * (d.y2 - d.y1);
            if (box_area > area_thres * img_area)
                continue;   // 剔除
        }
        after_large_filter.push_back(d);
    }

    if (after_large_filter.empty())
        return results;

    // 4. 排序（反转顺序）
// 原 Python 逻辑：np.lexsort((-boxes[:,7], boxes[:,6]))
// 反转后：先按 order_minor 升序，再按 order_major 降序
    std::sort(after_large_filter.begin(), after_large_filter.end(),
              [](const Detection& a, const Detection& b) {
                  if (a.order_minor != b.order_minor)
                      return a.order_minor < b.order_minor;   // 升序（反转）
                  return a.order_major > b.order_major;       // 降序（反转）
              });

    // 5. 裁剪坐标到图像边界，并构建 LayoutBox
    int order = 1;
    for (const auto& d : after_large_filter) {
        int x1 = static_cast<int>(std::max(0.0f, d.x1));
        int y1 = static_cast<int>(std::max(0.0f, d.y1));
        int x2 = static_cast<int>(std::min(static_cast<float>(image_size.width), d.x2));
        int y2 = static_cast<int>(std::min(static_cast<float>(image_size.height), d.y2));

        if (x2 <= x1 || y2 <= y1)
            continue;

        LayoutBox box;
        box.class_id = d.class_id;
        box.label = LABELS[d.class_id];
        box.score = d.score;
        box.box = cv::Rect(x1, y1, x2 - x1, y2 - y1);
        box.read_order = order++;   // 赋予排序序号

        results.push_back(box);
    }

    return results;
}

// ------------------------------------------------------------
// 推理入口
// ------------------------------------------------------------
bool PPDocLayoutV2::Init(const std::string& model_path, bool use_gpu, int device_id)
{
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(4);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    if (use_gpu) {
        // CUDA执行提供程序（启用GPU推理）
        OrtCUDAProviderOptions cuda_options;
        cuda_options.device_id = device_id;
        cuda_options.arena_extend_strategy = 0;
        cuda_options.gpu_mem_limit = 2ULL * 1024 * 1024 * 1024; // 2GB显存限制
        cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
        cuda_options.do_copy_in_default_stream = 1;
        options.AppendExecutionProvider_CUDA(cuda_options);
    }

#ifdef _WIN32
    std::wstring wpath(model_path.begin(), model_path.end());
    session_ = std::make_unique<Ort::Session>(env_, wpath.c_str(), options);
#else
    session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), options);
#endif
    return true;
}

std::vector<LayoutBox> PPDocLayoutV2::Detect(
        const cv::Mat& image,
        float score_threshold)
{
    std::vector<LayoutBox> results;
    if (image.empty())
        return results;

    // 预处理
    std::vector<float> im_shape, scale_factor;
    auto input_data = Preprocess(image, im_shape, scale_factor);

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<int64_t> image_shape = { 1, 3, INPUT_SIZE, INPUT_SIZE };
    std::vector<int64_t> vec_shape = { 1, 2 };

    auto image_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_data.data(), input_data.size(),
            image_shape.data(), image_shape.size());

    auto im_shape_tensor = Ort::Value::CreateTensor<float>(
            memory_info, im_shape.data(), im_shape.size(),
            vec_shape.data(), vec_shape.size());

    auto scale_tensor = Ort::Value::CreateTensor<float>(
            memory_info, scale_factor.data(), scale_factor.size(),
            vec_shape.data(), vec_shape.size());

    const char* input_names[] = { "im_shape", "image", "scale_factor" };
    const char* output_names[] = { "fetch_name_0", "fetch_name_1" };

    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(im_shape_tensor));
    inputs.push_back(std::move(image_tensor));
    inputs.push_back(std::move(scale_tensor));

    auto outputs = session_->Run(
            Ort::RunOptions{ nullptr },
            input_names,
            inputs.data(),
            inputs.size(),
            output_names,
            2
    );

    // 解析输出
    auto* dets = outputs[0].GetTensorMutableData<float>();
    auto* count = outputs[1].GetTensorMutableData<int32_t>();
    int n = static_cast<int>(count[0]);

    std::vector<Detection> raw_detections;
    raw_detections.reserve(n);

    for (int i = 0; i < n; ++i) {
        float* p = dets + i * 8;   // 每行 8 个值
        Detection d;
        d.class_id = static_cast<int>(p[0]);
        d.score = p[1];
        d.x1 = p[2];
        d.y1 = p[3];
        d.x2 = p[4];
        d.y2 = p[5];
        d.order_major = p[6];
        d.order_minor = p[7];
        raw_detections.push_back(d);
    }

    // 后处理
    results = PostProcess(raw_detections, image.size(), score_threshold);
    return results;
}