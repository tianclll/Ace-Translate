#include "docmind/engines/OCREngine.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace docmind {

    OCREngine::OCREngine(DLLLoader& loader, const std::string& model_dir, bool use_gpu)
            : dll_(loader) {
        if (!dll_.ocrLoaded()) {
            throw std::runtime_error("OCR DLL not loaded");
        }
        if (!dll_.ocrCreate || !dll_.ocrDestroy || !dll_.ocrRecognizeBuffer || !dll_.ocrFreeString) {
            throw std::runtime_error("OCR function pointers missing");
        }
        handle_ = dll_.ocrCreate(model_dir.c_str(), use_gpu ? 1 : 0);
        if (!handle_) {
            throw std::runtime_error("Failed to create OCR instance with model dir: " + model_dir);
        }
        initialized_ = true;
        std::cout << "OCREngine initialized." << std::endl;
    }

    OCREngine::~OCREngine() {
        if (handle_ && dll_.ocrDestroy) {
            dll_.ocrDestroy(handle_);
            handle_ = nullptr;
        }
    }

    std::vector<OCRResult> OCREngine::recognizeBuffer(const cv::Mat& image) {
        std::vector<OCRResult> results;
        if (!initialized_ || image.empty()) return results;
        if (!dll_.ocrRecognizeBuffer) return results;

        char* json = dll_.ocrRecognizeBuffer(
                handle_,
                image.data,
                image.cols,
                image.rows,
                image.channels()
        );
        if (json) {
            results = parseResults(json);
            dll_.ocrFreeString(json);
        }
        return results;
    }

    OCRResult OCREngine::recognizeCrop(const cv::Mat& crop) {
        OCRResult result;
        if (!initialized_ || crop.empty()) return result;

        // 根据高度选择合适函数（与原逻辑一致）
        char* json = nullptr;
        if (crop.rows < 100000) {
            if (dll_.ocrRecognizeCrop) {
                json = dll_.ocrRecognizeCrop(handle_, crop.data, crop.cols, crop.rows, crop.channels());
            } else {
                // 如果没有专用函数，则使用 buffer
                json = dll_.ocrRecognizeBuffer(handle_, crop.data, crop.cols, crop.rows, crop.channels());
            }
        } else {
            json = dll_.ocrRecognizeBuffer(handle_, crop.data, crop.cols, crop.rows, crop.channels());
        }

        if (json) {
            auto results = parseResults(json);
            if (!results.empty()) {
                result = results[0];
            }
            dll_.ocrFreeString(json);
        }
        return result;
    }

    std::vector<OCRResult> OCREngine::parseResults(const std::string& json) {
        std::vector<OCRResult> results;
        try {
            auto j = nlohmann::json::parse(json);
            if (j.contains("results")) {
                for (const auto& item : j["results"]) {
                    OCRResult res;
                    res.text = item["text"].get<std::string>();
                    res.score = item["score"].get<float>();
                    if (item.contains("box")) {
                        for (const auto& pt : item["box"]) {
                            res.box.push_back({pt[0].get<float>(), pt[1].get<float>()});
                        }
                    }
                    results.push_back(res);
                }
            } else if (j.contains("text") && j.contains("score")) {
                OCRResult res;
                res.text = j["text"].get<std::string>();
                res.score = j["score"].get<float>();
                results.push_back(res);
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse OCR JSON: " << e.what() << std::endl;
        }
        return results;
    }

} // namespace docmind