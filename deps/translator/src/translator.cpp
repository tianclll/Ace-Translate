#define TRANSLATOR_EXPORTS
#include "translator.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>

#include "llama.h"

// ============================================================
// Translator::Impl - 内部实现类
// ============================================================

class Translator::Impl {
public:
    Impl(
            const std::string& model_path,
            int n_gpu_layers
    );

    ~Impl();

    std::string translate(
            const std::string& source_text,
            const std::string& target_language,
            int max_tokens
    );

private:
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    llama_sampler* sampler = nullptr;

    int n_ctx = 512;
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

Translator::Impl::Impl(
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

Translator::Impl::~Impl() {
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
}

std::string Translator::Impl::token_to_piece(
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

std::string Translator::Impl::generate(
        const std::string& prompt,
        int max_tokens) {

    llama_memory_clear(llama_get_memory(ctx), true);

    //----------------------------------------
    // tokenize
    //----------------------------------------

    int n_tokens = -llama_tokenize(
            vocab,
            prompt.c_str(),
            static_cast<int>(prompt.size()),
            nullptr,
            0,
            true,
            true
    );

    if (n_tokens <= 0) {
        throw std::runtime_error("tokenize failed");
    }

    std::vector<llama_token> tokens(n_tokens);

    llama_tokenize(
            vocab,
            prompt.c_str(),
            static_cast<int>(prompt.size()),
            tokens.data(),
            static_cast<int>(tokens.size()),
            true,
            true
    );

    //----------------------------------------
    // decode prompt
    //----------------------------------------

    llama_batch batch = llama_batch_get_one(
            tokens.data(),
            static_cast<int>(tokens.size())
    );

    if (llama_decode(ctx, batch) != 0) {
        throw std::runtime_error("decode prompt failed");
    }

    //----------------------------------------
    // stop words
    //----------------------------------------

    static const std::vector<std::string> stop_words = {
            "<|im_end|>",
            "<|im_start|>",
            "<|eot_id|>",
            "<|endoftext|>",
            "User:",
            "Assistant:",
            "\nUser",
            "\nAssistant"
    };

    //----------------------------------------
    // generate
    //----------------------------------------

    std::string result;

    for (int i = 0; i < max_tokens; i++) {
        llama_token token = llama_sampler_sample(
                sampler,
                ctx,
                -1
        );

        //----------------------------------------
        // EOS
        //----------------------------------------

        if (llama_vocab_is_eog(vocab, token)) {
            break;
        }

        //----------------------------------------
        // token -> text
        //----------------------------------------

        std::string piece = token_to_piece(token);

        if (piece.empty()) {
            continue;
        }

        //----------------------------------------
        // 特殊token
        //----------------------------------------

        if (piece.find("<|") != std::string::npos) {
            break;
        }

        result += piece;

        //----------------------------------------
        // Stop Word
        //----------------------------------------

        bool stop = false;

        for (const auto& s : stop_words) {
            auto pos = result.find(s);

            if (pos != std::string::npos) {
                result.erase(pos);
                stop = true;
                break;
            }
        }

        if (stop) {
            break;
        }

        //----------------------------------------
        // 连续空行
        //----------------------------------------

        if (result.size() >= 2) {
            if (result[result.size() - 1] == '\n' &&
                result[result.size() - 2] == '\n') {
                break;
            }
        }

        //----------------------------------------
        // decode next
        //----------------------------------------

        batch = llama_batch_get_one(&token, 1);

        if (llama_decode(ctx, batch) != 0) {
            throw std::runtime_error("decode token failed");
        }
    }

    //----------------------------------------
    // trim
    //----------------------------------------

    auto begin = result.find_first_not_of(" \t\r\n");

    if (begin == std::string::npos) {
        return "";
    }

    auto end = result.find_last_not_of(" \t\r\n");

    return result.substr(begin, end - begin + 1);
}

std::string Translator::Impl::translate(
        const std::string& source_text,
        const std::string& target_language,
        int max_tokens) {

    std::string prompt =
            "请将下面文本翻译成" + target_language +
            "。\n"
            "只输出译文，不要解释。\n\n"
            "原文：\n"
            + source_text +
            "\n\n译文：";

    std::string result = generate(prompt, max_tokens);

    // 去掉首尾空白和换行
    auto begin = result.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    auto end = result.find_last_not_of(" \t\r\n");
    result = result.substr(begin, end - begin + 1);

    return result;
}

// ============================================================
// Translator 公共接口实现
// ============================================================

Translator::Translator(
        const std::string& model_path,
        int n_gpu_layers)
        : pImpl(std::make_unique<Impl>(model_path, n_gpu_layers)) {
}

Translator::~Translator() = default;

std::string Translator::translate(
        const std::string& source_text,
        const std::string& target_language,
        int max_tokens) {
    return pImpl->translate(source_text, target_language, max_tokens);
}

// ============================================================
// C 接口实现
// ============================================================

extern "C" {

TRANSLATOR_API TranslatorHandle translator_create(
        const char* model_path,
        int n_gpu_layers) {

    try {
        auto* translator = new Translator(
                std::string(model_path),
                n_gpu_layers
        );
        return static_cast<TranslatorHandle>(translator);
    } catch (const std::exception& e) {
        std::cerr << "创建翻译器失败: " << e.what() << std::endl;
        return nullptr;
    }
}

TRANSLATOR_API void translator_destroy(
        TranslatorHandle handle) {

    if (handle) {
        auto* translator = static_cast<Translator*>(handle);
        delete translator;
    }
}

TRANSLATOR_API const char* translator_translate(
        TranslatorHandle handle,
        const char* source_text,
        const char* target_language,
        int max_tokens) {

    if (!handle || !source_text || !target_language) {
        return nullptr;
    }

    try {
        auto* translator = static_cast<Translator*>(handle);
        std::string result = translator->translate(
                std::string(source_text),
                std::string(target_language),
                max_tokens
        );

        // 分配内存保存结果字符串
        char* str = new char[result.size() + 1];
        std::strcpy(str, result.c_str());
        return str;
    } catch (const std::exception& e) {
        std::cerr << "翻译失败: " << e.what() << std::endl;
        return nullptr;
    }
}

TRANSLATOR_API void translator_free_string(
        const char* str) {

    if (str) {
        delete[] str;
    }
}

}