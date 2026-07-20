#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ocr.hpp"
#include "predict_system.hpp"
#include "predict_det.hpp"   // 用于 OCRDetector
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ocr {

// ============================================================
// 编码转换辅助函数（仅 Windows）
// ============================================================
#ifdef _WIN32
    static std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (len <= 0) return std::wstring();
        std::wstring wstr(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        wstr.pop_back();
        return wstr;
    }

    static cv::Mat imread_utf8(const std::string& utf8_path) {
        std::wstring wpath = utf8_to_wstring(utf8_path);
        FILE* file = _wfopen(wpath.c_str(), L"rb");
        if (!file) return cv::Mat();
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        std::vector<unsigned char> buffer(file_size);
        fread(buffer.data(), 1, file_size, file);
        fclose(file);
        return cv::imdecode(buffer, cv::IMREAD_COLOR);
    }
#endif

// ============================================================
// Ocr::Impl
// ============================================================
    class Ocr::Impl {
    public:
        Impl(const std::string& model_dir, bool use_gpu) {
            model_dir_ = model_dir.empty() ? "./models" : model_dir;

            // 如果 model_dir 直接就是某个 size 的路径（如 models/ocr/tiny），
            // 直接在该目录下找 det/rec/cls
            // 如果是旧版的 models 根目录，则拼 /ocr/tiny/...
            std::string sep = "/";
            if (model_dir_.find('\\') != std::string::npos) sep = "\\";

            // 检查 model_dir/ocr/ 是否存在来判断是哪种传参方式
            std::string test_path = model_dir_ + sep + "det" + sep + "det.onnx";
            std::ifstream test_file(test_path);
            if (test_file.good()) {
                // model_dir 已经是 size 路径，直接使用
                det_model_path_ = model_dir_ + sep + "det" + sep + "det.onnx";
                rec_model_path_ = model_dir_ + sep + "rec" + sep + "rec.onnx";
                cls_model_path_ = model_dir_ + sep + "cls" + sep + "cls.onnx";
                // dict 在 model_dir 的上一级
                size_t pos = model_dir_.find_last_of("\\/");
                rec_dict_path_ = (pos != std::string::npos ? model_dir_.substr(0, pos) : model_dir_) + sep + "ppocrv6_tiny_dict.txt";
            } else {
                // 向后兼容：在 model_dir 后拼 /ocr/tiny
                det_model_path_ = model_dir_ + "/ocr/tiny/det/det.onnx";
                rec_model_path_ = model_dir_ + "/ocr/tiny/rec/rec.onnx";
                rec_dict_path_ = model_dir_ + "/ocr/ppocrv6_tiny_dict.txt";
                cls_model_path_ = model_dir_ + "/ocr/tiny/cls/cls.onnx";
            }

            // 检查文件存在性（仅打印）
            std::cout << "Model directory: " << model_dir_ << std::endl;
            std::cout << "  Detection: " << det_model_path_
                      << (std::ifstream(det_model_path_).good() ? " [OK]" : " [MISSING]") << std::endl;
            std::cout << "  Recognition: " << rec_model_path_
                      << (std::ifstream(rec_model_path_).good() ? " [OK]" : " [MISSING]") << std::endl;
            std::cout << "  Dictionary: " << rec_dict_path_
                      << (std::ifstream(rec_dict_path_).good() ? " [OK]" : " [MISSING]") << std::endl;
            std::cout << "  Classifier: " << cls_model_path_
                      << (std::ifstream(cls_model_path_).good() ? " [OK]" : " [MISSING]") << std::endl;

            use_gpu_ = use_gpu;
            ocr_system_ = std::make_unique<TextSystem>(
                    det_model_path_, rec_model_path_, rec_dict_path_,
                    cls_model_path_, use_gpu_
            );
            std::cout << "OCR system initialized." << std::endl;
        }

        std::vector<OCRResult> recognize(const cv::Mat& img) {
            return ocr_system_->recognize(img);
        }

        std::vector<OCRResult> recognize(const std::string& image_path) {
            cv::Mat img;
#ifdef _WIN32
            img = imread_utf8(image_path);
#else
            img = cv::imread(image_path, cv::IMREAD_COLOR);
#endif
            if (img.empty()) {
                throw std::runtime_error("Failed to load image: " + image_path);
            }
            return ocr_system_->recognize(img);
        }
        std::vector<OCRResult> recognizeCrop(const cv::Mat& img) {
            return ocr_system_->recognizeCrop(img);
        }

    private:
        std::string model_dir_;
        std::string det_model_path_;
        std::string rec_model_path_;
        std::string rec_dict_path_;
        std::string cls_model_path_;
        bool use_gpu_;
        std::unique_ptr<TextSystem> ocr_system_;
    };

    Ocr::Ocr(const std::string& model_dir, bool use_gpu)
            : pImpl(std::make_unique<Impl>(model_dir, use_gpu)) {}

    Ocr::~Ocr() = default;

    std::vector<OCRResult> Ocr::recognize(const cv::Mat& img) {
        return pImpl->recognize(img);
    }

    std::vector<OCRResult> Ocr::recognize(const std::string& image_path) {
        return pImpl->recognize(image_path);
    }
// 公共接口
    std::vector<OCRResult> Ocr::recognizeCrop(const cv::Mat& img) {
        return pImpl->recognizeCrop(img);}
// ============================================================
// OCRDetector::Impl
// ============================================================
    class OCRDetector::Impl {
    public:
        explicit Impl(const std::string& model_path, bool use_gpu)
                : detector_(model_path, use_gpu) {}

        std::vector<std::vector<cv::Point2f>> detect(const cv::Mat& img) {
            return detector_.detect(img);
        }

    private:
        TextDetector detector_;
    };

    OCRDetector::OCRDetector(const std::string& model_path, bool use_gpu)
            : pImpl(std::make_unique<Impl>(model_path, use_gpu)) {}

    OCRDetector::~OCRDetector() = default;

    std::vector<std::vector<cv::Point2f>> OCRDetector::detect(const cv::Mat& img) {
        return pImpl->detect(img);
    }

// ============================================================
// 辅助函数：将检测框转换为 JSON
// ============================================================
    static std::string boxes_to_json(const std::vector<std::vector<cv::Point2f>>& boxes) {
        std::ostringstream json;
        json << "{\"boxes\":[";
        for (size_t i = 0; i < boxes.size(); ++i) {
            if (i > 0) json << ",";
            json << "[";
            for (size_t j = 0; j < boxes[i].size(); ++j) {
                if (j > 0) json << ",";
                json << "[" << boxes[i][j].x << "," << boxes[i][j].y << "]";
            }
            json << "]";
        }
        json << "]}";
        return json.str();
    }

// ============================================================
// C 接口导出
// ============================================================
    extern "C" {

// ---------- OCR 完整识别 ----------
    OCR_API void* ocr_create(const char* model_dir, int use_gpu) {
        try {
            std::string dir = model_dir ? model_dir : "";
            bool gpu = (use_gpu != 0);
            auto* ocr = new ocr::Ocr(dir, gpu);
            return static_cast<void*>(ocr);
        } catch (const std::exception& e) {
            std::cerr << "ocr_create error: " << e.what() << std::endl;
            return nullptr;
        }
    }
// src/ocr.cpp - extern "C" 块中

    OCR_API char* ocr_recognize_crop(void* ocr, const unsigned char* data, int width, int height, int channels) {
        if (!ocr || !data || width <= 0 || height <= 0 || channels != 3) return nullptr;
        try {
            std::cout<<channels<<std::endl;
            cv::Mat img(height, width, CV_8UC3, const_cast<unsigned char*>(data));

            auto* ocr_obj = static_cast<ocr::Ocr*>(ocr);
            auto results = ocr_obj->recognizeCrop(img);

            std::ostringstream json;
            json << "{\"text\":\"" << results[0].text << "\",\"score\":" << results[0].score << "}";
            std::string s = json.str();
            char* result_str = new char[s.size() + 1];
            std::strcpy(result_str, s.c_str());
            return result_str;
        } catch (const std::exception& e) {
            std::cerr << "ocr_recognize_crop error: " << e.what() << std::endl;
            return nullptr;
        }
    }
    OCR_API void ocr_destroy(void* ocr) {
        if (ocr) delete static_cast<ocr::Ocr*>(ocr);
    }

    OCR_API char* ocr_recognize_file(void* ocr, const char* image_path) {
        if (!ocr || !image_path) return nullptr;
        try {
            auto* ocr_obj = static_cast<ocr::Ocr*>(ocr);
            auto results = ocr_obj->recognize(std::string(image_path));

            std::ostringstream json;
            json << "{\"results\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) json << ",";
                json << "{"
                     << "\"text\":\"" << results[i].text << "\","
                     << "\"score\":" << results[i].score << ","
                     << "\"box\":[";
                for (size_t j = 0; j < results[i].box.size(); ++j) {
                    if (j > 0) json << ",";
                    json << "[" << results[i].box[j][0] << "," << results[i].box[j][1] << "]";
                }
                json << "]}";
            }
            json << "]}";
            std::string s = json.str();
            char* result = new char[s.size() + 1];
            std::strcpy(result, s.c_str());
            return result;
        } catch (const std::exception& e) {
            std::cerr << "ocr_recognize_file error: " << e.what() << std::endl;
            return nullptr;
        }
    }

    OCR_API char* ocr_recognize_buffer(void* ocr, const unsigned char* data, int width, int height, int channels) {
        if (!ocr || !data || width <= 0 || height <= 0 || channels != 3) return nullptr;
        try {
            cv::Mat img(height, width, CV_8UC3, const_cast<unsigned char*>(data));

            auto* ocr_obj = static_cast<ocr::Ocr*>(ocr);
            auto results = ocr_obj->recognize(img);

            std::ostringstream json;
            json << "{\"results\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) json << ",";
                json << "{"
                     << "\"text\":\"" << results[i].text << "\","
                     << "\"score\":" << results[i].score << ","
                     << "\"box\":[";
                for (size_t j = 0; j < results[i].box.size(); ++j) {
                    if (j > 0) json << ",";
                    json << "[" << results[i].box[j][0] << "," << results[i].box[j][1] << "]";
                }
                json << "]}";
            }
            json << "]}";
            std::string s = json.str();
            char* result = new char[s.size() + 1];
            std::strcpy(result, s.c_str());
            return result;
        } catch (const std::exception& e) {
            std::cerr << "ocr_recognize_buffer error: " << e.what() << std::endl;
            return nullptr;
        }
    }

// ---------- 检测器 ----------
    OCR_API void* ocr_detector_create(const char* model_path, int use_gpu) {
        try {
            if (!model_path) return nullptr;
            auto* det = new ocr::OCRDetector(model_path, use_gpu != 0);
            return static_cast<void*>(det);
        } catch (const std::exception& e) {
            std::cerr << "ocr_detector_create error: " << e.what() << std::endl;
            return nullptr;
        }
    }

    OCR_API void ocr_detector_destroy(void* detector) {
        if (detector) delete static_cast<ocr::OCRDetector*>(detector);
    }

    OCR_API char* ocr_detector_detect_file(void* detector, const char* image_path) {
        if (!detector || !image_path) return nullptr;
        try {
            cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);
            if (img.empty()) return nullptr;
            auto* det = static_cast<ocr::OCRDetector*>(detector);
            auto boxes = det->detect(img);
            std::string json = boxes_to_json(boxes);
            char* result = new char[json.size() + 1];
            std::strcpy(result, json.c_str());
            return result;
        } catch (const std::exception& e) {
            std::cerr << "ocr_detector_detect_file error: " << e.what() << std::endl;
            return nullptr;
        }
    }

    OCR_API char* ocr_detector_detect_buffer(void* detector, const unsigned char* data, int width, int height, int channels) {
        if (!detector || !data || width <= 0 || height <= 0 || channels != 3) return nullptr;
        try {
            cv::Mat img(height, width, CV_8UC3, const_cast<unsigned char*>(data));
            auto* det = static_cast<ocr::OCRDetector*>(detector);
            auto boxes = det->detect(img);
            std::string json = boxes_to_json(boxes);
            char* result = new char[json.size() + 1];
            std::strcpy(result, json.c_str());
            return result;
        } catch (const std::exception& e) {
            std::cerr << "ocr_detector_detect_buffer error: " << e.what() << std::endl;
            return nullptr;
        }
    }

// ---------- 通用释放 ----------
    OCR_API void ocr_free_string(char* str) {
        if (str) delete[] str;
    }

    } // extern "C"

} // namespace ocr