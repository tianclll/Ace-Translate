#include "SenseVoiceDLL.h"
#include "SenseVoiceEngine.h"
#include "AudioCapture.h"
#include "Fbank.h"

#include <cstring>
#include <string>

// ============================================================
// DLL 全局锁（简单互斥）
// ============================================================
#include <mutex>
static std::mutex g_asr_mutex;

// ============================================================
// asr_create — 创建引擎实例
// ============================================================
ASR_API void* asr_create(const char* model_path, const char* tokens_path, const char* mvn_path, int use_gpu) {
    if (!model_path || !tokens_path) return nullptr;

    auto* engine = new SenseVoiceEngine();
    std::string mvn = mvn_path ? mvn_path : "";
    if (!engine->init(model_path, tokens_path, mvn, use_gpu != 0)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

// ============================================================
// asr_recognize — 识别音频
// ============================================================
ASR_API char* asr_recognize(void* handle, const short* pcm, int pcm_len) {
    if (!handle || !pcm || pcm_len <= 0) return nullptr;

    auto* engine = static_cast<SenseVoiceEngine*>(handle);

    std::lock_guard<std::mutex> lock(g_asr_mutex);

    // 1. 提取 fbank 特征
    Fbank fbank;
    std::vector<float> feats = fbank.extract(pcm, pcm_len);
    int num_frames = fbank.frameCount();
    if (feats.empty() || num_frames <= 0) {
        char* empty = new char[1];
        empty[0] = '\0';
        return empty;
    }

    // 2. 推理
    std::string text = engine->recognize(feats.data(), num_frames);

    // 3. 分配返回字符串（调用者 free）
    char* result = new char[text.size() + 1];
    memcpy(result, text.c_str(), text.size() + 1);
    return result;
}

// ============================================================
// asr_destroy — 销毁引擎
// ============================================================
ASR_API void asr_destroy(void* handle) {
    if (!handle) return;
    auto* engine = static_cast<SenseVoiceEngine*>(handle);
    delete engine;
}
