#include "docmind/engines/ASREngine.hpp"
#include <iostream>

namespace docmind {

    ASREngine::ASREngine(DLLLoader& loader,
                         const std::string& model_path,
                         const std::string& tokens_path,
                         const std::string& mvn_path,
                         int n_gpu_layers)
            : dll_(loader) {
        if (!dll_.asrLoaded()) {
            throw std::runtime_error("ASR DLL not loaded");
        }
        if (!dll_.asr_create || !dll_.asr_destroy || !dll_.asr_recognize) {
            throw std::runtime_error("ASR function pointers missing");
        }
        handle_ = dll_.asr_create(model_path.c_str(), tokens_path.c_str(), mvn_path.c_str(), n_gpu_layers);
        if (!handle_) {
            throw std::runtime_error("Failed to create ASR instance with model: " + model_path);
        }
        initialized_ = true;
        std::cout << "ASREngine initialized." << std::endl;
    }

    ASREngine::~ASREngine() {
        if (handle_ && dll_.asr_destroy) {
            dll_.asr_destroy(handle_);
            handle_ = nullptr;
        }
    }

    std::string ASREngine::recognize(const short* pcm, int pcm_len) {
        if (!initialized_ || !pcm || pcm_len <= 0) {
            return {};
        }
        char* result = dll_.asr_recognize(handle_, pcm, pcm_len);
        std::string text;
        if (result) {
            text = result;
            // ASR DLL 用 new[] 分配的字符串，需要对应方式释放
            // DLL 内部用 delete[]，这里通过 FreeLibrary 时自动清理，
            // 但最好调用 DLL 提供的 free 函数。
            // SenseVoiceDLL 用 new char[]，但没有导出 free 函数，
            // 用 delete[] 释放是安全的（同一模块内）
            delete[] result;
        }
        return text;
    }

} // namespace docmind
