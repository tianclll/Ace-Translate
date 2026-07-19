#include "docmind/engines/LayoutDetector.hpp"
#include <iostream>

namespace docmind {

    LayoutDetector::LayoutDetector(DLLLoader& loader, const std::string& model_path, bool use_gpu, int device_id)
            : dll_(loader) {
        if (!dll_.layoutLoaded()) {
            throw std::runtime_error("Layout DLL not loaded");
        }
        if (!dll_.createLayout || !dll_.initLayout || !dll_.destroyLayout) {
            throw std::runtime_error("Layout function pointers missing");
        }
        handle_ = dll_.createLayout();
        if (!handle_) {
            throw std::runtime_error("Failed to create layout instance");
        }
        if (!dll_.initLayout(handle_, model_path.c_str(), use_gpu, device_id)) {
            dll_.destroyLayout(handle_);
            handle_ = nullptr;
            throw std::runtime_error("Layout init failed with model: " + model_path);
        }
        initialized_ = true;
        std::cout << "LayoutDetector initialized." << std::endl;
    }

    LayoutDetector::~LayoutDetector() {
        if (handle_ && dll_.destroyLayout) {
            dll_.destroyLayout(handle_);
            handle_ = nullptr;
        }
    }

    std::vector<BoxInfo> LayoutDetector::detect(const cv::Mat& image, float score_threshold) {
        std::vector<BoxInfo> boxes;
        if (!initialized_ || !dll_.detectLayout) {
            std::cerr << "LayoutDetector not ready for detection." << std::endl;
            return boxes;
        }
        if (image.empty()) {
            return boxes;
        }

        const int MAX_BOXES = 256;
        LayoutBox layout_results[MAX_BOXES];
        int count = dll_.detectLayout(
                handle_,
                image.data,
                image.cols,
                image.rows,
                image.channels(),
                layout_results,
                MAX_BOXES
        );

        if (count <= 0) {
            std::cout << "No layout boxes detected." << std::endl;
            return boxes;
        }

        for (int i = 0; i < count; ++i) {
            const auto& lb = layout_results[i];
            if (lb.width <= 0 || lb.height <= 0 || lb.score < score_threshold) {
                continue;
            }
            BoxInfo info;
            info.bbox = cv::Rect(lb.x, lb.y, lb.width, lb.height);
            info.class_id = lb.class_id;
            info.label = std::string(lb.label);
            info.score = lb.score;
            boxes.push_back(info);
        }

        std::cout << "LayoutDetector: " << boxes.size() << " boxes detected." << std::endl;
        return boxes;
    }

} // namespace docmind