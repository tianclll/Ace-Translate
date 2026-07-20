#pragma once

#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>
#include "CMVN.h"

/**
 * @brief SenseVoice ONNX 推理引擎
 * 输入 fbank 特征 → ONNX 推理 → CTC 解码 → 文本
 */
class SenseVoiceEngine {
public:
    SenseVoiceEngine();
    ~SenseVoiceEngine() = default;

    /** @brief 初始化模型 */
    bool init(const std::string& model_path, const std::string& tokens_path, const std::string& mvn_path, bool use_gpu);

    /** @brief 识别音频
     *  @param fbank 560-dim fbank 特征 [frames * 560]
     *  @param num_frames 帧数
     *  @return 识别文本
     */
    std::string recognize(const float* fbank, int num_frames);

    /** @brief 是否就绪 */
    bool isReady() const { return session_ != nullptr; }

private:
    // ONNX 相关
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "SenseVoice"};
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memory_info_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};

    // 输入输出名称
    std::string input_name_;
    std::string length_name_;
    std::string lang_name_;
    std::string norm_name_;
    std::string output_name_;
    std::string outlen_name_;

    // Tokenizer
    std::vector<std::string> tokens_;     // id → text
    int blank_id_ = 0;

    // CMVN
    CMVN cmvn_;

    /** @brief CTC 贪婪解码 */
    std::string ctcDecode(const float* logits, int time_steps, int vocab_size);

    /** @brief 加载 tokens.json */
    bool loadTokens(const std::string& path);
};
