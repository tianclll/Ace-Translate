#pragma once

#include <memory>
#include <string>
#include <mutex>
#include "docmind/engines/DLLLoader.hpp"
#include "docmind/engines/LayoutDetector.hpp"
#include "docmind/engines/OCREngine.hpp"
#include "docmind/engines/VLMEngine.hpp"
#include "docmind/engines/TranslatorEngine.hpp"

namespace docmind {

    class GlobalEngineContext {
    public:
        static GlobalEngineContext& getInstance();

        // 初始化（使用配置管理器加载的路径）
        void initialize(const std::string& base_dir = "");

        // 获取引擎指针
        DLLLoader* getDLLLoader() { return dll_loader_.get(); }
        LayoutDetector* getLayoutDetector() { return layout_.get(); }
        OCREngine* getOCREngine() { return ocr_.get(); }
        VLMEngine* getVLMEngine() { return vlm_.get(); }
        TranslatorEngine* getTranslatorEngine() { return translator_.get(); }

        // 获取 DocumentImageProcessor 句柄（若已加载）
        void* getDocProcHandle() { return docproc_handle_; }

        // ASR 引擎
        void* getASRHandle() { return asr_handle_; }
        bool isASRLoaded() const { return asr_loaded_; }
        void setBaseDir(const std::string& dir) { base_dir_ = dir; }

        bool isInitialized() const { return initialized_; }

    private:
        GlobalEngineContext() = default;
        ~GlobalEngineContext() = default;
        GlobalEngineContext(const GlobalEngineContext&) = delete;
        GlobalEngineContext& operator=(const GlobalEngineContext&) = delete;

        std::unique_ptr<DLLLoader> dll_loader_;
        std::unique_ptr<LayoutDetector> layout_;
        std::unique_ptr<OCREngine> ocr_;
        std::unique_ptr<VLMEngine> vlm_;
        std::unique_ptr<TranslatorEngine> translator_;
        void* docproc_handle_ = nullptr;
        bool docproc_loaded_ = false;
        bool initialized_ = false;
        std::once_flag init_flag_;

        // ASR
        void* asr_handle_ = nullptr;
        bool asr_loaded_ = false;
        std::string base_dir_;
    };

} // namespace docmind