#include "SenseVoiceEngine.h"
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <codecvt>
#include <locale>

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
        // tokens.json is a JSON array of strings
        char c;
        file >> c; // skip [
        if (c != '[') return false;

        std::string token;
        bool in_string = false;
        while (file.get(c)) {
            if (c == '\"') {
                in_string = !in_string;
                if (!in_string) {
                    // end of token
                    tokens_.push_back(token);
                    token.clear();
                    // skip comma or whitespace
                    while (file.get(c) && (c == ',' || c == ' ' || c == '\n' || c == '\r'));
                    if (c == ']') break;
                    // put back
                    file.unget();
                }
            } else if (in_string) {
                // handle escaped chars
                if (c == '\\') {
                    file.get(c);
                    if (c == 'u') {
                        // unicode escape - skip for simplicity
                        token += '?';
                        for (int i = 0; i < 4; ++i) file.get(c);
                    } else {
                        token += c;
                    }
                } else {
                    token += c;
                }
            }
        }
    } catch (...) {
        return false;
    }

    return !tokens_.empty();
}

bool SenseVoiceEngine::init(const std::string& model_path, const std::string& tokens_path, bool use_gpu) {
    // 加载 tokenizer
    if (!loadTokens(tokens_path)) {
        return false;
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
        session_ = std::make_unique<Ort::Session>(env_, wpath.c_str(), options);
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
        // 构建输入张量
        // speech: [1, num_frames, 560]
        std::vector<int64_t> speech_shape = {1, num_frames, 560};
        int64_t speech_size = 1LL * num_frames * 560;

        auto speech_tensor = Ort::Value::CreateTensor<float>(
            memory_info_,
            const_cast<float*>(fbank),  // ONNX 不修改输入
            speech_size,
            speech_shape.data(),
            speech_shape.size()
        );

        // speech_lengths: [1]
        std::vector<int64_t> length_shape = {1};
        std::vector<int64_t> length_data = {num_frames};
        auto length_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info_,
            length_data.data(),
            1,
            length_shape.data(),
            length_shape.size()
        );

        // language: [1] (0 = auto detect)
        std::vector<int64_t> lang_shape = {1};
        std::vector<int64_t> lang_data = {0};
        auto lang_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info_,
            lang_data.data(),
            1,
            lang_shape.data(),
            lang_shape.size()
        );

        // textnorm: [1] (1 = with textnorm)
        std::vector<int64_t> norm_shape = {1};
        std::vector<int64_t> norm_data = {1};
        auto norm_tensor = Ort::Value::CreateTensor<int64_t>(
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

        // CTC 解码
        std::string text = ctcDecode(reinterpret_cast<const int64_t*>(logits_data), actual_len, vocab_size);
        return text;

    } catch (const std::exception& e) {
        return std::string("[error: ") + e.what() + "]";
    }
}

std::string SenseVoiceEngine::ctcDecode(const int64_t* logits, int time_steps, int vocab_size) {
    if (!logits || time_steps <= 0) return {};

    std::string result;
    int prev_id = blank_id_;

    // 每一帧取 argmax
    for (int t = 0; t < time_steps; ++t) {
        const float* frame = reinterpret_cast<const float*>(logits) + t * vocab_size;
        int max_id = 0;
        float max_val = frame[0];
        for (int i = 1; i < vocab_size; ++i) {
            if (frame[i] > max_val) {
                max_val = frame[i];
                max_id = i;
            }
        }

        // 去重 + 跳过 blank
        if (max_id != blank_id_ && max_id != prev_id) {
            if (max_id >= 0 && max_id < (int)tokens_.size() && max_id > 2) {
                // token 0=unk, 1=<s>, 2=</s>
                result += tokens_[max_id];
            }
        }
        prev_id = max_id;
    }

    // 移除 BPE 标记 ▁ → 空格
    // SenseVoice 使用 SentencePiece BPE，▁ 表示词的开头
    std::string final_text;
    for (size_t i = 0; i < result.size(); ) {
        // UTF-8 解码 ▁ (U+2581, UTF-8: E2 96 81)
        if ((unsigned char)result[i] == 0xE2 && i + 2 < result.size() &&
            (unsigned char)result[i+1] == 0x96 && (unsigned char)result[i+2] == 0x81) {
            final_text += ' ';
            i += 3;
        } else {
            // 跳过 <s> </s> 等特殊 token
            if (result[i] == '<') {
                while (i < result.size() && result[i] != '>') ++i;
                ++i; // skip >
            } else {
                final_text += result[i];
                ++i;
            }
        }
    }

    // 去掉首尾空格
    size_t start = final_text.find_first_not_of(' ');
    size_t end = final_text.find_last_not_of(' ');
    if (start != std::string::npos && end != std::string::npos) {
        final_text = final_text.substr(start, end - start + 1);
    } else if (start == std::string::npos) {
        final_text.clear();
    }

    return final_text;
}
