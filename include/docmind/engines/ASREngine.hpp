#pragma once

#include "DLLLoader.hpp"
#include <string>
#include <vector>

namespace docmind {

    class ASREngine {
    public:
        ASREngine(DLLLoader& loader,
                  const std::string& model_path,
                  const std::string& tokens_path,
                  const std::string& mvn_path,
                  int n_gpu_layers = 0);
        ~ASREngine();

        /** @brief 是否已加载 */
        bool isLoaded() const { return handle_ != nullptr; }

        /** @brief 识别 PCM 音频
         *  @param pcm 16kHz 16-bit mono PCM 数据
         *  @param pcm_len 采样点数
         *  @return 识别文本
         */
        std::string recognize(const short* pcm, int pcm_len);

    private:
        DLLLoader& dll_;
        void* handle_ = nullptr;
        bool initialized_ = false;
    };

} // namespace docmind
