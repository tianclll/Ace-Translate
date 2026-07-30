#pragma once

#include "DLLLoader.hpp"
#include <string>

namespace docmind {

    class SummarizerEngine {
    public:
        SummarizerEngine(DLLLoader& loader,
                         const std::string& model_path,
                         int n_gpu_layers = 0);
        ~SummarizerEngine();

        bool isLoaded() const { return handle_ != nullptr; }

        /// 生成中文摘要
        std::string summarize(const std::string& text, int max_tokens = 256);

    private:
        DLLLoader& dll_;
        void* handle_ = nullptr;
        bool initialized_ = false;
    };

} // namespace docmind
