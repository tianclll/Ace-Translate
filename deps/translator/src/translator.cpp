#define TRANSLATOR_EXPORTS
#include "translator.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <unordered_map>
#include <algorithm>

#include "llama.h"
#include <windows.h>

// ============================================================
// 语言名称 → 中文名映射（供 prompt 使用）
// ============================================================
static const std::unordered_map<std::string, std::string> langToChinese = {
    {"zh", "中文"},
    {"Chinese", "中文"},
    {"中文", "中文"},
    {"中国語", "中文"},
    {"en", "英语"},
    {"English", "英语"},
    {"英语", "英语"},
    {"英語", "英语"},
    {"fr", "法语"},
    {"French", "法语"},
    {"法语", "法语"},
    {"フランス語", "法语"},
    {"pt", "葡萄牙语"},
    {"Portuguese", "葡萄牙语"},
    {"葡萄牙语", "葡萄牙语"},
    {"ポルトガル語", "葡萄牙语"},
    {"es", "西班牙语"},
    {"Spanish", "西班牙语"},
    {"西班牙语", "西班牙语"},
    {"スペイン語", "西班牙语"},
    {"ja", "日语"},
    {"Japanese", "日语"},
    {"日本語", "日语"},
    {"日语", "日语"},
    {"tr", "土耳其语"},
    {"Turkish", "土耳其语"},
    {"土耳其语", "土耳其语"},
    {"トルコ語", "土耳其语"},
    {"ru", "俄语"},
    {"Russian", "俄语"},
    {"俄语", "俄语"},
    {"ロシア語", "俄语"},
    {"ar", "阿拉伯语"},
    {"Arabic", "阿拉伯语"},
    {"阿拉伯语", "阿拉伯语"},
    {"アラビア語", "阿拉伯语"},
    {"ko", "韩语"},
    {"Korean", "韩语"},
    {"한국어", "韩语"},
    {"韩语", "韩语"},
    {"韓国語", "韩语"},
    {"th", "泰语"},
    {"Thai", "泰语"},
    {"泰语", "泰语"},
    {"タイ語", "泰语"},
    {"it", "意大利语"},
    {"Italian", "意大利语"},
    {"意大利语", "意大利语"},
    {"イタリア語", "意大利语"},
    {"de", "德语"},
    {"German", "德语"},
    {"德语", "德语"},
    {"ドイツ語", "德语"},
    {"vi", "越南语"},
    {"Vietnamese", "越南语"},
    {"越南语", "越南语"},
    {"ベトナム語", "越南语"},
    {"ms", "马来语"},
    {"Malay", "马来语"},
    {"马来语", "马来语"},
    {"マレー語", "马来语"},
    {"id", "印尼语"},
    {"Indonesian", "印尼语"},
    {"印尼语", "印尼语"},
    {"インドネシア語", "印尼语"},
    {"tl", "菲律宾语"},
    {"Filipino", "菲律宾语"},
    {"菲律宾语", "菲律宾语"},
    {"フィリピノ語", "菲律宾语"},
    {"hi", "印地语"},
    {"Hindi", "印地语"},
    {"印地语", "印地语"},
    {"ヒンディー語", "印地语"},
    {"zh-Hant", "繁体中文"},
    {"Chinese (Traditional)", "繁体中文"},
    {"繁体中文", "繁体中文"},
    {"繁体字中国語", "繁体中文"},
    {"pl", "波兰语"},
    {"Polish", "波兰语"},
    {"波兰语", "波兰语"},
    {"ポーランド語", "波兰语"},
    {"cs", "捷克语"},
    {"Czech", "捷克语"},
    {"捷克语", "捷克语"},
    {"チェコ語", "捷克语"},
    {"nl", "荷兰语"},
    {"Dutch", "荷兰语"},
    {"荷兰语", "荷兰语"},
    {"オランダ語", "荷兰语"},
    {"km", "高棉语"},
    {"Khmer", "高棉语"},
    {"高棉语", "高棉语"},
    {"クメール語", "高棉语"},
    {"my", "缅甸语"},
    {"Myanmar (Burmese)", "缅甸语"},
    {"缅甸语", "缅甸语"},
    {"ビルマ語", "缅甸语"},
    {"fa", "波斯语"},
    {"Persian", "波斯语"},
    {"波斯语", "波斯语"},
    {"ペルシア語", "波斯语"},
    {"gu", "古吉拉特语"},
    {"Gujarati", "古吉拉特语"},
    {"古吉拉特语", "古吉拉特语"},
    {"グジャラート語", "古吉拉特语"},
    {"ur", "乌尔都语"},
    {"Urdu", "乌尔都语"},
    {"乌尔都语", "乌尔都语"},
    {"ウルドゥー語", "乌尔都语"},
    {"te", "泰卢固语"},
    {"Telugu", "泰卢固语"},
    {"泰卢固语", "泰卢固语"},
    {"テルグ語", "泰卢固语"},
    {"mr", "马拉地语"},
    {"Marathi", "马拉地语"},
    {"马拉地语", "马拉地语"},
    {"マラーティー語", "马拉地语"},
    {"he", "希伯来语"},
    {"Hebrew", "希伯来语"},
    {"希伯来语", "希伯来语"},
    {"ヘブライ語", "希伯来语"},
    {"bn", "孟加拉语"},
    {"Bengali", "孟加拉语"},
    {"孟加拉语", "孟加拉语"},
    {"ベンガル語", "孟加拉语"},
    {"ta", "泰米尔语"},
    {"Tamil", "泰米尔语"},
    {"泰米尔语", "泰米尔语"},
    {"タミル語", "泰米尔语"},
    {"uk", "乌克兰语"},
    {"Ukrainian", "乌克兰语"},
    {"乌克兰语", "乌克兰语"},
    {"ウクライナ語", "乌克兰语"},
    {"bo", "藏语"},
    {"Tibetan", "藏语"},
    {"藏语", "藏语"},
    {"チベット語", "藏语"},
    {"kk", "哈萨克语"},
    {"Kazakh", "哈萨克语"},
    {"哈萨克语", "哈萨克语"},
    {"カザフ語", "哈萨克语"},
    {"mn", "蒙古语"},
    {"Mongolian", "蒙古语"},
    {"蒙古语", "蒙古语"},
    {"モンゴル語", "蒙古语"},
    {"ug", "维吾尔语"},
    {"Uyghur", "维吾尔语"},
    {"维吾尔语", "维吾尔语"},
    {"ウイグル語", "维吾尔语"},
    {"yue", "粤语"},
    {"Cantonese", "粤语"},
    {"粤语", "粤语"},
    {"広東語", "粤语"},
};

static std::string toChineseLangName(const std::string& lang) {
    auto it = langToChinese.find(lang);
    if (it != langToChinese.end()) return it->second;
    // 没找到就直接用原文
    return lang;
}

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

// ============================================================
// raw_generate_seh — 纯 C 辅助函数，__try 内没有任何 C++ 对象
// 所有缓冲区由调用者通过指针传入
// ============================================================
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
        llama_token tokens[4096];
        int n_tokens = llama_tokenize(
                vocab,
                prompt_text,
                prompt_len,
                tokens,
                4096,
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

std::string Translator::Impl::generate(
        const std::string& prompt,
        int max_tokens) {

    char buf[4096 * 4];  // 16KB 输出缓冲区
    int outLen = 0;

    if (!raw_generate_seh(ctx, vocab, sampler,
                           prompt.c_str(), (int)prompt.size(),
                           max_tokens, buf, sizeof(buf), &outLen)) {
        std::cerr << "generate() SEH crashed or failed" << std::endl;
        std::cerr.flush();
        return "";
    }

    return std::string(buf, outLen);
}

std::string Translator::Impl::translate(
        const std::string& source_text,
        const std::string& target_language,
        int max_tokens) {

    std::string lang_cn = toChineseLangName(target_language);
    std::string prompt =
            "请将下面文本翻译成" + lang_cn +
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
// summarize — 用摘要 prompt 调用底层 generate
// ============================================================
std::string Translator::Impl::summarize(
        const std::string& source_text,
        int max_tokens) {

    std::string prompt =
            "请将下面文本用一句话概括成中文摘要。\n"
            "只输出摘要，不要解释。\n\n"
            "原文：\n"
            + source_text +
            "\n\n摘要：";

    std::string result = generate(prompt, max_tokens);

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

std::string Translator::summarize(
        const std::string& source_text,
        int max_tokens) {
    return pImpl->summarize(source_text, max_tokens);
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

TRANSLATOR_API const char* translator_summarize(
        TranslatorHandle handle,
        const char* source_text,
        int max_tokens) {

    if (!handle || !source_text) {
        return nullptr;
    }

    try {
        auto* translator = static_cast<Translator*>(handle);
        std::string result = translator->summarize(
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

TRANSLATOR_API void translator_free_string(
        const char* str) {

    if (str) {
        delete[] str;
    }
}

}