#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SenseVoiceEngine.h"
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <codecvt>
#include <locale>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
static std::wstring toWide(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wstr[0], len);
    return wstr;
}
#endif

SenseVoiceEngine::SenseVoiceEngine() {}

bool SenseVoiceEngine::loadTokens(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;

        tokens_.clear();
        tokens_.reserve(j.size());

        for (auto& item : j)
            tokens_.push_back(item.get<std::string>());

        // 动态检测 blank_id：检查 tokens_.size() - 1 是否经常被 argmax 选中
        // 初始假设 blank = 0（<unk>），推理时会根据第一帧自动校准
        blank_id_ = 0;
    } catch (...) {
        return false;
    }

    return !tokens_.empty();
}

bool SenseVoiceEngine::init(const std::string& model_path, const std::string& tokens_path, const std::string& mvn_path, bool use_gpu) {
    // 加载 tokenizer
    if (!loadTokens(tokens_path)) {
        return false;
    }

    // 加载 CMVN
    if (!mvn_path.empty() && !cmvn_.load(mvn_path)) {
        std::cerr << "Warning: CMVN not loaded, recognition may be poor" << std::endl;
    }

    // ONNX session 配置
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(4);
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.EnableCpuMemArena();
    options.EnableMemPattern();

    if (use_gpu) {
        OrtCUDAProviderOptions cuda_option;
        cuda_option.device_id = 0;
        cuda_option.arena_extend_strategy = 0;
        cuda_option.gpu_mem_limit = 2ULL * 1024 * 1024 * 1024;
        cuda_option.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
        cuda_option.do_copy_in_default_stream = 1;
        options.AppendExecutionProvider_CUDA(cuda_option);
    }

    try {
#ifdef _WIN32
        std::wstring wpath = toWide(model_path);
        try {
            session_ = std::make_unique<Ort::Session>(env_, wpath.c_str(), options);
        } catch (const std::exception& e) {
            std::cerr << "SenseVoice session create error: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "SenseVoice session create unknown error" << std::endl;
            return false;
        }
#else
        session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), options);
#endif
    } catch (const std::exception& e) {
        session_ = nullptr;
        return false;
    }

    // 获取输入输出名称
    auto alloc = Ort::AllocatorWithDefaultOptions();
    {
        auto name = session_->GetInputNameAllocated(0, alloc);
        input_name_ = name.get();
    }
    {
        auto name = session_->GetInputNameAllocated(1, alloc);
        length_name_ = name.get();
    }
    {
        auto name = session_->GetInputNameAllocated(2, alloc);
        lang_name_ = name.get();
    }
    {
        auto name = session_->GetInputNameAllocated(3, alloc);
        norm_name_ = name.get();
    }
    {
        auto name = session_->GetOutputNameAllocated(0, alloc);
        output_name_ = name.get();
    }
    {
        auto name = session_->GetOutputNameAllocated(1, alloc);
        outlen_name_ = name.get();
    }

    return true;
}

std::string SenseVoiceEngine::recognize(const float* fbank, int num_frames) {
    if (!session_ || !fbank || num_frames <= 0) return {};

    try {
        // 复制 fbank 数据用于 CMVN（不能修改原始 const 输入）
        std::vector<float> feats(fbank, fbank + num_frames * 560LL);
        if (cmvn_.isLoaded()) {
            cmvn_.apply(feats, num_frames, 560);
        }

        // 构建输入张量
        // speech: [1, num_frames, 560]
        std::vector<int64_t> speech_shape = {1, num_frames, 560};
        int64_t speech_size = 1LL * num_frames * 560;

        auto speech_tensor = Ort::Value::CreateTensor<float>(
            memory_info_,
            feats.data(),  // 使用 CMVN 后的数据
            speech_size,
            speech_shape.data(),
            speech_shape.size()
        );

        // speech_lengths: [1]
        std::vector<int64_t> length_shape = {1};
        std::vector<int32_t> length_data = {num_frames};
        auto length_tensor = Ort::Value::CreateTensor<int32_t>(
            memory_info_,
            length_data.data(),
            1,
            length_shape.data(),
            length_shape.size()
        );

        // language: [1] (0 = auto detect)
        std::vector<int64_t> lang_shape = {1};
        std::vector<int32_t> lang_data = {0};
        auto lang_tensor = Ort::Value::CreateTensor<int32_t>(
            memory_info_,
            lang_data.data(),
            1,
            lang_shape.data(),
            lang_shape.size()
        );

        // textnorm: [1] (1 = with textnorm)
        std::vector<int64_t> norm_shape = {1};
        std::vector<int32_t> norm_data = {1};
        auto norm_tensor = Ort::Value::CreateTensor<int32_t>(
            memory_info_,
            norm_data.data(),
            1,
            norm_shape.data(),
            norm_shape.size()
        );

        // 构建输入
        const char* input_names[] = {
            input_name_.c_str(),
            length_name_.c_str(),
            lang_name_.c_str(),
            norm_name_.c_str()
        };
        Ort::Value input_tensors[] = {
            std::move(speech_tensor),
            std::move(length_tensor),
            std::move(lang_tensor),
            std::move(norm_tensor)
        };

        const char* output_names[] = {
            output_name_.c_str(),
            outlen_name_.c_str()
        };

        // 推理
        auto result = session_->Run(
            Ort::RunOptions{nullptr},
            input_names,
            input_tensors,
            4,
            output_names,
            2
        );

        if (result.size() < 2) {
            return std::string("[error: ASR output missing]");
        }

        // 提取 CTC logits
        const float* logits_data = result[0].GetTensorData<float>();
        auto logits_shape = result[0].GetTensorTypeAndShapeInfo().GetShape();
        const int64_t* out_len_data = result[1].GetTensorData<int64_t>();

        int batch_time = (int)logits_shape[1];
        int vocab_size = (int)logits_shape[2];
        int actual_len = (out_len_data && logits_shape.size() > 1) ? (int)out_len_data[0] : batch_time;
        // 防止越界
        if (actual_len > batch_time) actual_len = batch_time;
        if (actual_len <= 0) actual_len = batch_time;

        // CTC 解码
        std::string text = ctcDecode(logits_data, actual_len, vocab_size);
        return text;

    } catch (const std::exception& e) {
        return std::string("[error: ") + e.what() + "]";
    }
}

std::string SenseVoiceEngine::ctcDecode(const float* logits, int time_steps, int vocab_size) {
    if (!logits || time_steps <= 0) return {};

    // CTC blank 固定为 vocab_size - 1（SenseVoice 模型约定）
    blank_id_ = vocab_size - 1;

    std::vector<int> ids;
    int prev_id = blank_id_;

    for (int t = 0; t < time_steps; ++t) {
        const float* frame = logits + t * vocab_size;
        int max_id = (int)(std::max_element(frame, frame + vocab_size) - frame);

        // 去重 + 跳过 blank
        if (max_id != blank_id_ && max_id != prev_id) {
            if (max_id >= 0 && max_id < (int)tokens_.size()) {
                ids.push_back(max_id);
            }
        }
        prev_id = max_id;
    }

    // 用 SentencePiece 风格拼接：▁ → 空格
    std::string result;
    for (int id : ids) {
        const std::string& tk = tokens_[id];
        if (tk.empty()) continue;
        // 跳过特殊 token（<unk>, <s>, </s>, <|zh|>, <|en|> 等）
        if (tk[0] == '<') continue;
        // ▁（U+2581, UTF-8: E2 96 81）→ 空格
        if (tk.size() >= 3 && (unsigned char)tk[0] == 0xE2 &&
            (unsigned char)tk[1] == 0x96 && (unsigned char)tk[2] == 0x81) {
            result += ' ';
            result += tk.substr(3);
        } else {
            result += tk;
        }
    }

    // 去掉首尾空格
    size_t start = result.find_first_not_of(' ');
    size_t end = result.find_last_not_of(' ');
    if (start != std::string::npos && end != std::string::npos) {
        result = result.substr(start, end - start + 1);
    } else if (start == std::string::npos) {
        result.clear();
    }

    return result;
}
