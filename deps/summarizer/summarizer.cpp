#define SUMMARIZER_EXPORTS
#include "summarizer.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <unordered_map>
#include <algorithm>

#include "llama.h"
#include <windows.h>
#include <eh.h>

// ============================================================
// Summarizer::Impl - 内部实现类
// ============================================================

class Summarizer::Impl {
public:
    Impl(
            const std::string& model_path,
            int n_gpu_layers
    );

    ~Impl();

    std::string summarize(
            const std::string& source_text,
            int max_tokens
    );

private:
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    llama_sampler* sampler = nullptr;

    int n_ctx = 4096;
    int n_threads = 16;

    std::string generate(
            const std::string& prompt,
            int max_tokens
    );

    std::string token_to_piece(
            llama_token token
    );
};

// ============================================================
// Impl 实现
// ============================================================

Summarizer::Impl::Impl(
        const std::string& model_path,
        int n_gpu_layers) {

    std::cout << "\n========== GPU STATUS ==========" << std::endl;

    // 检查是否编译了CUDA支持
#ifdef GGML_USE_CUDA
    std::cout << "✅ llama.cpp compiled with CUDA support" << std::endl;
    std::cout << "   GPU layers requested: " << n_gpu_layers << std::endl;
#else
    std::cout << "❌ llama.cpp compiled WITHOUT CUDA support!" << std::endl;
    std::cout << "   Please recompile with -DGGML_USE_CUDA=ON" << std::endl;
    if (n_gpu_layers > 0) {
        std::cout << "   WARNING: GPU layers requested but CUDA not available!" << std::endl;
        std::cout << "   Falling back to CPU mode" << std::endl;
        n_gpu_layers = 0;
    }
#endif

    std::cout << "=================================" << std::endl;

    llama_backend_init();

    //-----------------------------------
    // model - 支持GPU
    //-----------------------------------

    auto model_params = llama_model_default_params();
    model_params.n_gpu_layers = n_gpu_layers;

    std::cout << "Loading model with " << n_gpu_layers << " GPU layers..." << std::endl;

    model = llama_model_load_from_file(
            model_path.c_str(),
            model_params
    );

    if (!model) {
        throw std::runtime_error("load model failed");
    }

    //-----------------------------------
    // vocab
    //-----------------------------------

    vocab = llama_model_get_vocab(model);

    //-----------------------------------
    // context
    //-----------------------------------

    auto ctx_params = llama_context_default_params();
    ctx_params.n_ctx = n_ctx;
    ctx_params.n_threads = n_threads;
    ctx_params.n_threads_batch = n_threads;

    ctx = llama_init_from_model(
            model,
            ctx_params
    );

    if (!ctx) {
        throw std::runtime_error("create context failed");
    }

    //-----------------------------------
    // sampler
    //-----------------------------------

    auto sparams = llama_sampler_chain_default_params();
    sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    std::cout << "模型加载完成！" << std::endl;
    if (n_gpu_layers > 0) {
        std::cout << "GPU已启用，使用 " << n_gpu_layers << " 层" << std::endl;
        std::cout << "注意：如果KV缓存显示为CPU，这是正常的，" << std::endl;
        std::cout << "因为llama.cpp默认将KV缓存放在CPU上。" << std::endl;
    } else {
        std::cout << "使用CPU模式" << std::endl;
    }
}

Summarizer::Impl::~Impl() {
    // 用 SEH 保护析构，防止 llama_free 在 GPU 状态下抛出异常
    __try {
        if (sampler) {
            llama_sampler_free(sampler);
        }
        if (ctx) {
            llama_free(ctx);
        }
        if (model) {
            llama_model_free(model);
        }
        llama_backend_free();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cerr << "~Impl() SEH crash during cleanup" << std::endl;
    }
}

std::string Summarizer::Impl::token_to_piece(
        llama_token token) {

    char buf[256]{};
    int n = llama_token_to_piece(
            vocab,
            token,
            buf,
            sizeof(buf),
            0,
            true
    );

    if (n <= 0) {
        return "";
    }
    return std::string(buf, n);
}

// ============================================================
// raw_generate_seh — 纯 C 辅助函数，__try 内没有任何 C++ 对象
// 禁用 /GS 安全检查（栈缓冲区溢出由 __except 处理）
// ============================================================
#pragma strict_gs_check(off)
static bool raw_generate_seh(
        llama_context* ctx,
        const llama_vocab* vocab,
        llama_sampler* sampler,
        const char* prompt_text,
        int prompt_len,
        int max_tokens,
        char* out_buf,
        int out_buf_size,
        int* out_len) {

    __try {
        llama_memory_clear(llama_get_memory(ctx), true);

        // 用固定大小数组，避免 std::vector（C++ 对象）
        llama_token tokens[8192];
        int n_tokens = llama_tokenize(
                vocab,
                prompt_text,
                prompt_len,
                tokens,
                2048,
                true,
                true
        );

        if (n_tokens <= 0) {
            return false;
        }

        // decode prompt
        llama_batch batch = llama_batch_get_one(tokens, n_tokens);

        if (llama_decode(ctx, batch) != 0) {
            return false;
        }

        // generate
        char accum[4096 * 4];  // 16KB 累积缓冲区
        int accum_len = 0;

        for (int i = 0; i < max_tokens && accum_len < (int)sizeof(accum) - 256; i++) {
            llama_token token = llama_sampler_sample(
                    sampler,
                    ctx,
                    -1
            );

            if (llama_vocab_is_eog(vocab, token)) {
                break;
            }

            char piece_buf[256];
            int n = llama_token_to_piece(
                    vocab,
                    token,
                    piece_buf,
                    sizeof(piece_buf),
                    0,
                    true
            );

            if (n <= 0) continue;

            // 检查特殊 token
            if (n >= 3 && piece_buf[0] == '<' && piece_buf[1] == '|') {
                break;
            }

            // 追加到 accum
            if (accum_len + n < (int)sizeof(accum)) {
                memcpy(accum + accum_len, piece_buf, n);
                accum_len += n;

                // 检查 stop words（纯 C 字符串查找）
                accum[accum_len] = '\0';
                const char* sw_found = nullptr;
                if ((sw_found = strstr(accum, "<|im_end|>"))) {
                    accum_len = (int)(sw_found - accum);
                    break;
                }
                if ((sw_found = strstr(accum, "<|im_start|>"))) {
                    accum_len = (int)(sw_found - accum);
                    break;
                }
                if ((sw_found = strstr(accum, "<|eot_id|>"))) {
                    accum_len = (int)(sw_found - accum);
                    break;
                }
                if ((sw_found = strstr(accum, "<|endoftext|>"))) {
                    accum_len = (int)(sw_found - accum);
                    break;
                }
                if ((sw_found = strstr(accum, "User:"))) {
                    accum_len = (int)(sw_found - accum);
                    break;
                }
                if ((sw_found = strstr(accum, "Assistant:"))) {
                    accum_len = (int)(sw_found - accum);
                    break;
                }
            }

            // decode next
            batch = llama_batch_get_one(&token, 1);
            if (llama_decode(ctx, batch) != 0) {
                return false;
            }
        }

        // trim
        int start = 0, end = accum_len;
        while (start < end && (accum[start] == ' ' || accum[start] == '\t' ||
               accum[start] == '\r' || accum[start] == '\n')) start++;
        while (end > start && (accum[end-1] == ' ' || accum[end-1] == '\t' ||
               accum[end-1] == '\r' || accum[end-1] == '\n')) end--;

        int result_len = end - start;
        if (result_len >= out_buf_size) {
            result_len = out_buf_size - 1;
        }
        if (result_len > 0) {
            memcpy(out_buf, accum + start, result_len);
        }
        out_buf[result_len] = '\0';
        *out_len = result_len;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::string Summarizer::Impl::generate(
        const std::string& prompt,
        int max_tokens) {

    // 注册 SEH 转换器（配合 /EHa，将 SEH 转为 C++ 异常）
    _set_se_translator([](unsigned int code, EXCEPTION_POINTERS*) {
        throw std::runtime_error("SEH error: " + std::to_string(code));
    });

    char buf[4096 * 4];  // 16KB 输出缓冲区
    int outLen = 0;

    try {
        if (!raw_generate_seh(ctx, vocab, sampler,
                               prompt.c_str(), (int)prompt.size(),
                               max_tokens, buf, sizeof(buf), &outLen)) {
            return "";
        }
    } catch (const std::exception&) {
        std::cerr << "generate() SEH crashed" << std::endl;
        std::cerr.flush();
        return "";
    }

    return std::string(buf, outLen);
}

// ============================================================
// summarize — 用摘要 prompt 调用底层 generate
// ============================================================
std::string Summarizer::Impl::summarize(
        const std::string& source_text,
        int max_tokens) {

    std::string prompt =
            "<|im_start|>system\n"
            "你是一个文本摘要助手。将用户提供的文本概括成一段80-150字的中文摘要。"
            "只输出摘要本身，不要输出任何其他内容。\n"
            "<|im_end|>\n"
            "<|im_start|>user\n"
            + source_text +
            "\n<|im_end|>\n"
            "<|im_start|>assistant\n摘要：";

    std::string result = generate(prompt, max_tokens);

    // trim
    auto begin = result.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    auto end = result.find_last_not_of(" \t\r\n");
    result = result.substr(begin, end - begin + 1);

    return result;
}

// ============================================================
// Summarizer 公共接口实现
// ============================================================

Summarizer::Summarizer(
        const std::string& model_path,
        int n_gpu_layers)
        : pImpl(std::make_unique<Impl>(model_path, n_gpu_layers)) {
}

Summarizer::~Summarizer() = default;

std::string Summarizer::summarize(
        const std::string& source_text,
        int max_tokens) {
    return pImpl->summarize(source_text, max_tokens);
}

// ============================================================
// C 接口实现
// ============================================================

extern "C" {

SUMMARIZER_API SummarizerHandle summarizer_create(
        const char* model_path,
        int n_gpu_layers) {

    try {
        auto* summarizer = new Summarizer(
                std::string(model_path),
                n_gpu_layers
        );
        return static_cast<SummarizerHandle>(summarizer);
    } catch (const std::exception& e) {
        std::cerr << "创建摘要器失败: " << e.what() << std::endl;
        return nullptr;
    }
}

SUMMARIZER_API void summarizer_destroy(
        SummarizerHandle handle) {

    if (handle) {
        auto* summarizer = static_cast<Summarizer*>(handle);
        delete summarizer;
    }
}

SUMMARIZER_API const char* summarizer_summarize(
        SummarizerHandle handle,
        const char* source_text,
        int max_tokens) {

    if (!handle || !source_text) {
        return nullptr;
    }

    try {
        auto* summarizer = static_cast<Summarizer*>(handle);
        std::string result = summarizer->summarize(
                std::string(source_text),
                max_tokens
        );

        char* str = new char[result.size() + 1];
        std::strcpy(str, result.c_str());
        return str;
    } catch (const std::exception& e) {
        std::cerr << "摘要生成失败: " << e.what() << std::endl;
        return nullptr;
    }
}

SUMMARIZER_API void summarizer_free_string(
        const char* str) {

    if (str) {
        delete[] str;
    }
}

}