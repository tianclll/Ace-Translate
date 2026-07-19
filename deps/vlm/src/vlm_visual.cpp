#include "vlm_visual.h"

// MTMD 多模态支持
#include "tools/mtmd/mtmd.h"
#include "tools/mtmd/mtmd-helper.h"

// common 工具
#include "common.h"
#include "sampling.h"
#include "chat.h"
#include "console.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cstdint>

// ============================================================
// 辅助函数
// ============================================================

static std::vector<unsigned char> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

// ============================================================
// VLMVisual::Impl - 内部实现类
// ============================================================

class VLMVisual::Impl {
public:
    Impl(
            const std::string& model_path,
            const std::string& mmproj_path,
            int n_threads,
            int n_ctx,
            int n_gpu_layers
    );
    ~Impl();

    std::string understand(
            const std::string& image_path,
            const std::string& prompt,
            int max_tokens
    );

    std::string understand_from_memory(
            const unsigned char* image_data,
            size_t data_size,
            const std::string& prompt,
            int max_tokens
    );

    std::string get_last_error() const { return m_last_error; }

private:
    llama_model* m_model = nullptr;
    llama_context* m_ctx = nullptr;
    const llama_vocab* m_vocab = nullptr;
    llama_sampler* m_sampler = nullptr;

    mtmd_context* m_mtmd_ctx = nullptr;

    int m_n_ctx = 8192;
    int m_n_threads = 4;
    int m_n_batch = 512;
    int m_n_gpu_layers = 0;
    llama_pos m_n_past = 0;
    std::string m_last_error;

    llama_token m_eos_token = -1;
    std::string m_media_marker;

    common_chat_templates_ptr m_tmpls;
    std::vector<common_chat_msg> m_chat_history;
    bool m_use_jinja = false;

    std::string generate_response(int max_tokens);
    int eval_message(const std::string& prompt, const std::string& image_path);
    int eval_message_memory(const std::string& prompt,const unsigned char* image_data,size_t data_size);
    std::vector<llama_token> tokenize(const std::string& text);

    void set_error(const std::string& msg) {
        m_last_error = msg;
        std::cerr << "VLMVisual Error: " << msg << std::endl;
    }
};

// ============================================================
// Impl 构造/析构
// ============================================================

VLMVisual::Impl::Impl(
        const std::string& model_path,
        const std::string& mmproj_path,
        int n_threads,
        int n_ctx,
        int n_gpu_layers
) : m_n_threads(n_threads > 0 ? n_threads : 4),
    m_n_ctx(n_ctx > 0 ? n_ctx : 8192),
    m_n_gpu_layers(n_gpu_layers),
    m_n_batch(512) {

    std::cout << "\n========== VLM GPU STATUS ==========" << std::endl;

    // 检查是否编译了CUDA支持
#ifdef GGML_USE_CUDA
    std::cout << "✅ llama.cpp compiled with CUDA support" << std::endl;
    std::cout << "   GPU layers requested: " << m_n_gpu_layers << std::endl;
#else
    std::cout << "❌ llama.cpp compiled WITHOUT CUDA support!" << std::endl;
    if (m_n_gpu_layers > 0) {
        std::cout << "   WARNING: GPU layers requested but CUDA not available!" << std::endl;
        std::cout << "   Falling back to CPU mode" << std::endl;
        m_n_gpu_layers = 0;
    }
#endif
    std::cout << "=====================================" << std::endl;

    llama_backend_init();

    //-----------------------------------
    // 加载模型 - 支持GPU
    //-----------------------------------

    auto model_params = llama_model_default_params();
    model_params.n_gpu_layers = m_n_gpu_layers;  // GPU层数控制

    std::cout << "Loading model with " << m_n_gpu_layers << " GPU layers..." << std::endl;

    m_model = llama_load_model_from_file(model_path.c_str(), model_params);
    if (!m_model) {
        throw std::runtime_error("Failed to load model: " + model_path);
    }

    m_vocab = llama_model_get_vocab(m_model);
    m_eos_token = llama_vocab_eos(m_vocab);

    //-----------------------------------
    // 创建上下文
    //-----------------------------------

    auto ctx_params = llama_context_default_params();
    ctx_params.n_ctx = m_n_ctx;
    ctx_params.n_threads = m_n_threads;
    ctx_params.n_threads_batch = m_n_threads;

    m_ctx = llama_init_from_model(m_model, ctx_params);
    if (!m_ctx) {
        llama_free_model(m_model);
        throw std::runtime_error("Failed to create context");
    }

    //-----------------------------------
    // 创建采样器
    //-----------------------------------

    auto sparams = llama_sampler_chain_default_params();
    m_sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(m_sampler, llama_sampler_init_greedy());

    // ============================================================
    // 加载 MTMD 多模态模型
    // ============================================================
    if (!mmproj_path.empty()) {
        std::cout << "加载 mmproj: " << mmproj_path << std::endl;

        auto mtmd_params = mtmd_context_params_default();
        mtmd_params.use_gpu = (m_n_gpu_layers > 0);  // 如果模型用GPU，mmproj也用GPU
        mtmd_params.n_threads = m_n_threads;
        mtmd_params.print_timings = true;
        mtmd_params.warmup = true;

        m_mtmd_ctx = mtmd_init_from_file(
                mmproj_path.c_str(),
                m_model,
                mtmd_params
        );

        if (!m_mtmd_ctx) {
            llama_free(m_ctx);
            llama_free_model(m_model);
            throw std::runtime_error("Failed to load MTMD model: " + mmproj_path);
        }

        m_media_marker = mtmd_get_marker(m_mtmd_ctx);
        if (m_media_marker.empty()) {
            m_media_marker = mtmd_default_marker();
        }

        std::cout << "mmproj 加载成功！" << std::endl;
        std::cout << "  - 媒体标记: " << m_media_marker << std::endl;
        std::cout << "  - mmproj GPU: " << (mtmd_params.use_gpu ? "启用" : "禁用") << std::endl;
    }

    common_params params;
    params.chat_template = "";
    m_tmpls = common_chat_templates_init(m_model, params.chat_template);
    m_use_jinja = params.use_jinja;

    std::cout << "VLMVisual 模型加载完成！" << std::endl;
    std::cout << "  - 上下文大小: " << m_n_ctx << std::endl;
    std::cout << "  - 线程数: " << m_n_threads << std::endl;
    std::cout << "  - GPU层数: " << m_n_gpu_layers << (m_n_gpu_layers > 0 ? " (GPU加速)" : " (CPU模式)") << std::endl;
}

VLMVisual::Impl::~Impl() {
    if (m_sampler) {
        llama_sampler_free(m_sampler);
    }
    if (m_mtmd_ctx) {
        mtmd_free(m_mtmd_ctx);
        m_mtmd_ctx = nullptr;
    }
    if (m_ctx) {
        llama_free(m_ctx);
    }
    if (m_model) {
        llama_free_model(m_model);
    }
    llama_backend_free();
}

// ============================================================
// Tokenize
// ============================================================

std::vector<llama_token> VLMVisual::Impl::tokenize(const std::string& text) {
    std::vector<llama_token> tokens;
    if (!m_vocab || text.empty()) {
        return tokens;
    }

    int n_tokens = llama_tokenize(
            m_vocab,
            text.c_str(),
            static_cast<int32_t>(text.length()),
            nullptr,
            0,
            true,
            true
    );

    if (n_tokens <= 0) {
        return tokens;
    }

    tokens.resize(n_tokens);
    n_tokens = llama_tokenize(
            m_vocab,
            text.c_str(),
            static_cast<int32_t>(text.length()),
            tokens.data(),
            static_cast<int32_t>(tokens.size()),
            true,
            true
    );

    if (n_tokens <= 0) {
        tokens.clear();
    } else {
        tokens.resize(n_tokens);
    }

    return tokens;
}

// ============================================================
// eval_message
// ============================================================

int VLMVisual::Impl::eval_message(const std::string& prompt, const std::string& image_path) {
    common_chat_msg msg;
    msg.role = "user";

    std::string full_prompt = m_media_marker + "\n" + prompt;
    msg.content = full_prompt;

    bool add_bos = m_chat_history.empty();
    auto formatted_chat = common_chat_format_single(
            m_tmpls.get(),
            m_chat_history,
            msg,
            true,
            m_use_jinja
    );
    m_chat_history.push_back(msg);

    auto res = mtmd_helper_bitmap_init_from_file(m_mtmd_ctx, image_path.c_str(), false);
    if (!res.bitmap) {
        set_error("Failed to load image: " + image_path);
        return 1;
    }

    mtmd_input_text text;
    text.text = formatted_chat.c_str();
    text.add_special = add_bos;
    text.parse_special = true;

    mtmd_input_chunks* chunks = mtmd_input_chunks_init();
    const mtmd_bitmap* bitmaps[] = { res.bitmap };
    int32_t ret = mtmd_tokenize(
            m_mtmd_ctx,
            chunks,
            &text,
            bitmaps,
            1
    );

    mtmd_bitmap_free(res.bitmap);

    if (ret != 0) {
        mtmd_input_chunks_free(chunks);
        set_error("mtmd_tokenize failed: " + std::to_string(ret));
        return 1;
    }

    size_t n_chunks = mtmd_input_chunks_size(chunks);
    mtmd_batch* mbatch = nullptr;

    for (size_t i = 0; i < n_chunks; i++) {
        const mtmd_input_chunk* chunk = mtmd_input_chunks_get(chunks, i);
        auto chunk_type = mtmd_input_chunk_get_type(chunk);

        if (chunk_type == MTMD_INPUT_CHUNK_TYPE_TEXT) {
            llama_pos new_n_past = m_n_past;
            ret = mtmd_helper_eval_chunk_single(
                    m_mtmd_ctx,
                    m_ctx,
                    chunk,
                    m_n_past,
                    0,
                    m_n_batch,
                    i == n_chunks - 1,
                    &new_n_past
            );
            if (ret != 0) {
                mtmd_input_chunks_free(chunks);
                if (mbatch) mtmd_batch_free(mbatch);
                set_error("Failed to eval text chunk");
                return 1;
            }
            m_n_past = new_n_past;
        } else {
            float* embd = nullptr;

            if (mbatch) {
                embd = mtmd_batch_get_output_embd(mbatch, chunk);
            }

            if (!embd) {
                if (mbatch) {
                    mtmd_batch_free(mbatch);
                }
                mbatch = mtmd_batch_init(m_mtmd_ctx);
                ret = mtmd_batch_add_chunk(mbatch, chunk);
                if (ret != 0) {
                    mtmd_input_chunks_free(chunks);
                    mtmd_batch_free(mbatch);
                    set_error("Failed to add chunk to batch");
                    return 1;
                }

                for (size_t j = i + 1; j < n_chunks; j++) {
                    const mtmd_input_chunk* next_chunk = mtmd_input_chunks_get(chunks, j);
                    auto next_type = mtmd_input_chunk_get_type(next_chunk);
                    if (next_type == MTMD_INPUT_CHUNK_TYPE_TEXT) {
                        break;
                    }
                    ret = mtmd_batch_add_chunk(mbatch, next_chunk);
                    if (ret != 0) {
                        break;
                    }
                }

                ret = mtmd_batch_encode(mbatch);
                if (ret != 0) {
                    mtmd_input_chunks_free(chunks);
                    mtmd_batch_free(mbatch);
                    set_error("Failed to encode mtmd batch");
                    return 1;
                }

                embd = mtmd_batch_get_output_embd(mbatch, chunk);
            }

            if (!embd) {
                mtmd_input_chunks_free(chunks);
                if (mbatch) mtmd_batch_free(mbatch);
                set_error("Failed to get embeddings for image chunk");
                return 1;
            }

            llama_pos new_n_past = m_n_past;
            ret = mtmd_helper_decode_image_chunk(
                    m_mtmd_ctx,
                    m_ctx,
                    chunk,
                    embd,
                    m_n_past,
                    0,
                    m_n_batch,
                    &new_n_past,
                    nullptr,
                    nullptr
            );
            if (ret != 0) {
                mtmd_input_chunks_free(chunks);
                if (mbatch) mtmd_batch_free(mbatch);
                set_error("Failed to decode image chunk");
                return 1;
            }
            m_n_past = new_n_past;
        }
    }

    mtmd_input_chunks_free(chunks);
    if (mbatch) mtmd_batch_free(mbatch);

    return 0;
}

int VLMVisual::Impl::eval_message_memory(
        const std::string& prompt,
        const unsigned char* image_data,
        size_t data_size) {
    common_chat_msg msg;
    msg.role = "user";

    std::string full_prompt = m_media_marker + "\n" + prompt;
    msg.content = full_prompt;

    bool add_bos = m_chat_history.empty();
    auto formatted_chat = common_chat_format_single(
            m_tmpls.get(),
            m_chat_history,
            msg,
            true,
            m_use_jinja
    );
    m_chat_history.push_back(msg);
    auto res = mtmd_helper_bitmap_init_from_buf(
            m_mtmd_ctx,
            image_data,
            data_size,
            false);
    if (!res.bitmap) {
        return 1;
    }

    mtmd_input_text text;
    text.text = formatted_chat.c_str();
    text.add_special = add_bos;
    text.parse_special = true;

    mtmd_input_chunks* chunks = mtmd_input_chunks_init();
    const mtmd_bitmap* bitmaps[] = { res.bitmap };
    int32_t ret = mtmd_tokenize(
            m_mtmd_ctx,
            chunks,
            &text,
            bitmaps,
            1
    );

    mtmd_bitmap_free(res.bitmap);

    if (ret != 0) {
        mtmd_input_chunks_free(chunks);
        set_error("mtmd_tokenize failed: " + std::to_string(ret));
        return 1;
    }

    size_t n_chunks = mtmd_input_chunks_size(chunks);
    mtmd_batch* mbatch = nullptr;

    for (size_t i = 0; i < n_chunks; i++) {
        const mtmd_input_chunk* chunk = mtmd_input_chunks_get(chunks, i);
        auto chunk_type = mtmd_input_chunk_get_type(chunk);

        if (chunk_type == MTMD_INPUT_CHUNK_TYPE_TEXT) {
            llama_pos new_n_past = m_n_past;
            ret = mtmd_helper_eval_chunk_single(
                    m_mtmd_ctx,
                    m_ctx,
                    chunk,
                    m_n_past,
                    0,
                    m_n_batch,
                    i == n_chunks - 1,
                    &new_n_past
            );
            if (ret != 0) {
                mtmd_input_chunks_free(chunks);
                if (mbatch) mtmd_batch_free(mbatch);
                set_error("Failed to eval text chunk");
                return 1;
            }
            m_n_past = new_n_past;
        } else {
            float* embd = nullptr;

            if (mbatch) {
                embd = mtmd_batch_get_output_embd(mbatch, chunk);
            }

            if (!embd) {
                if (mbatch) {
                    mtmd_batch_free(mbatch);
                }
                mbatch = mtmd_batch_init(m_mtmd_ctx);
                ret = mtmd_batch_add_chunk(mbatch, chunk);
                if (ret != 0) {
                    mtmd_input_chunks_free(chunks);
                    mtmd_batch_free(mbatch);
                    set_error("Failed to add chunk to batch");
                    return 1;
                }

                for (size_t j = i + 1; j < n_chunks; j++) {
                    const mtmd_input_chunk* next_chunk = mtmd_input_chunks_get(chunks, j);
                    auto next_type = mtmd_input_chunk_get_type(next_chunk);
                    if (next_type == MTMD_INPUT_CHUNK_TYPE_TEXT) {
                        break;
                    }
                    ret = mtmd_batch_add_chunk(mbatch, next_chunk);
                    if (ret != 0) {
                        break;
                    }
                }

                ret = mtmd_batch_encode(mbatch);
                if (ret != 0) {
                    mtmd_input_chunks_free(chunks);
                    mtmd_batch_free(mbatch);
                    set_error("Failed to encode mtmd batch");
                    return 1;
                }

                embd = mtmd_batch_get_output_embd(mbatch, chunk);
            }

            if (!embd) {
                mtmd_input_chunks_free(chunks);
                if (mbatch) mtmd_batch_free(mbatch);
                set_error("Failed to get embeddings for image chunk");
                return 1;
            }

            llama_pos new_n_past = m_n_past;
            ret = mtmd_helper_decode_image_chunk(
                    m_mtmd_ctx,
                    m_ctx,
                    chunk,
                    embd,
                    m_n_past,
                    0,
                    m_n_batch,
                    &new_n_past,
                    nullptr,
                    nullptr
            );
            if (ret != 0) {
                mtmd_input_chunks_free(chunks);
                if (mbatch) mtmd_batch_free(mbatch);
                set_error("Failed to decode image chunk");
                return 1;
            }
            m_n_past = new_n_past;
        }
    }

    mtmd_input_chunks_free(chunks);
    if (mbatch) mtmd_batch_free(mbatch);

    return 0;
}

// ============================================================
// generate_response
// ============================================================

std::string VLMVisual::Impl::generate_response(int max_tokens) {
    std::string result;
    std::vector<llama_token> generated_tokens;

    int available_tokens = m_n_ctx - m_n_past - 1;
    if (available_tokens <= 0) {
        set_error("No available context slots");
        return result;
    }

    int actual_max_tokens = std::min(max_tokens, available_tokens);

    for (int i = 0; i < actual_max_tokens; i++) {
        llama_token token_id = llama_sampler_sample(m_sampler, m_ctx, -1);
        generated_tokens.push_back(token_id);
        llama_sampler_accept(m_sampler, token_id);

        if (llama_vocab_is_eog(m_vocab, token_id) || token_id == m_eos_token) {
            break;
        }

        std::string piece;
        char buf[128];
        int n = llama_token_to_piece(m_vocab, token_id, buf, sizeof(buf), 0, true);
        if (n > 0) {
            piece = std::string(buf, n);
        }
        result += piece;
        std::cout << piece << std::flush;

        llama_batch batch = llama_batch_get_one(&token_id, 1);
        if (llama_decode(m_ctx, batch)) {
            set_error("Failed to decode token");
            return result;
        }
        m_n_past++;
    }

    common_chat_msg msg;
    msg.role = "assistant";
    msg.content = result;
    m_chat_history.push_back(msg);

    return result;
}

// ============================================================
// 公开接口实现
// ============================================================

std::string VLMVisual::Impl::understand(
        const std::string& image_path,
        const std::string& prompt,
        int max_tokens) {

    set_error("");

    try {
        if (!m_mtmd_ctx) {
            set_error("MTMD not initialized");
            return "";
        }

        m_n_past = 0;
        m_chat_history.clear();
        llama_memory_clear(llama_get_memory(m_ctx), true);

        int ret = eval_message(prompt, image_path);
        if (ret != 0) {
            return "";
        }

        std::string result = generate_response(max_tokens);
        return result;

    } catch (const std::exception& e) {
        set_error(e.what());
        return "";
    }
}

std::string VLMVisual::Impl::understand_from_memory(
        const unsigned char* image_data,
        size_t data_size,
        const std::string& prompt,
        int max_tokens)
{
    set_error("");

    try {

        if (!m_mtmd_ctx) {
            set_error("MTMD not initialized");
            return "";
        }

        m_n_past = 0;
        m_chat_history.clear();

        llama_memory_clear(
                llama_get_memory(m_ctx),
                true);

        int ret = eval_message_memory(
                prompt,
                image_data,
                data_size);

        if (ret != 0)
            return "";

        return generate_response(max_tokens);

    } catch (const std::exception& e) {
        set_error(e.what());
        return "";
    }
}

// ============================================================
// VLMVisual 公共接口
// ============================================================

VLMVisual::VLMVisual(
        const std::string& model_path,
        const std::string& mmproj_path,
        int n_threads,
        int n_ctx,
        int n_gpu_layers
) : pImpl(std::make_unique<Impl>(model_path, mmproj_path, n_threads, n_ctx, n_gpu_layers)) {
}

VLMVisual::~VLMVisual() = default;

std::string VLMVisual::understand(
        const std::string& image_path,
        const std::string& prompt,
        int max_tokens) {
    return pImpl->understand(image_path, prompt, max_tokens);
}

std::string VLMVisual::understand_from_memory(
        const unsigned char* image_data,
        size_t data_size,
        const std::string& prompt,
        int max_tokens) {
    return pImpl->understand_from_memory(image_data, data_size, prompt, max_tokens);
}

std::string VLMVisual::get_last_error() const {
    return pImpl->get_last_error();
}

// ============================================================
// C 接口实现
// ============================================================

extern "C" {

VLM_VISUAL_API VLMHandle vlm_create(
        const char* model_path,
        const char* mmproj_path,
        int n_threads,
        int n_ctx,
        int n_gpu_layers) {

    try {
        std::cout << "vlm_create: model_path = " << (model_path ? model_path : "null") << std::endl;
        std::cout << "vlm_create: mmproj_path = " << (mmproj_path ? mmproj_path : "null") << std::endl;
        std::cout << "vlm_create: n_gpu_layers = " << n_gpu_layers << std::endl;

        if (!model_path || strlen(model_path) == 0) {
            std::cerr << "vlm_create: model_path is empty or null" << std::endl;
            return nullptr;
        }

        auto* vlm = new VLMVisual(
                std::string(model_path),
                mmproj_path ? std::string(mmproj_path) : "",
                n_threads > 0 ? n_threads : 4,
                n_ctx > 0 ? n_ctx : 8192,
                n_gpu_layers >= 0 ? n_gpu_layers : 0
        );
        return static_cast<VLMHandle>(vlm);
    } catch (const std::exception& e) {
        std::cerr << "创建 VLM 实例失败: " << e.what() << std::endl;
        return nullptr;
    }
}

VLM_VISUAL_API void vlm_destroy(VLMHandle handle) {
    if (handle) {
        auto* vlm = static_cast<VLMVisual*>(handle);
        delete vlm;
    }
}

VLM_VISUAL_API const char* vlm_understand(
        VLMHandle handle,
        const char* image_path,
        const char* prompt,
        int max_tokens) {

    if (!handle || !image_path || !prompt) {
        return nullptr;
    }

    try {
        auto* vlm = static_cast<VLMVisual*>(handle);
        std::string result = vlm->understand(
                std::string(image_path),
                std::string(prompt),
                max_tokens > 0 ? max_tokens : 2048
        );

        char* str = new char[result.size() + 1];
        std::strcpy(str, result.c_str());
        return str;
    } catch (const std::exception& e) {
        std::cerr << "视觉理解失败: " << e.what() << std::endl;
        return nullptr;
    }
}

VLM_VISUAL_API const char* vlm_understand_from_memory(
        VLMHandle handle,
        const unsigned char* image_data,
        size_t data_size,
        const char* prompt,
        int max_tokens) {
    if (!handle || !image_data || !prompt)
        return nullptr;

    try {
        auto* vlm = static_cast<VLMVisual*>(handle);

        std::string result = vlm->understand_from_memory(
                image_data,
                data_size,
                prompt,
                max_tokens > 0 ? max_tokens : 2048
        );

        char* str = new char[result.size() + 1];
        strcpy(str, result.c_str());
        return str;
    } catch (const std::exception& e) {
        std::cerr << "视觉理解失败: " << e.what() << std::endl;
        return nullptr;
    }
}

VLM_VISUAL_API void vlm_free_string(const char* str) {
    if (str) {
        delete[] str;
    }
}

VLM_VISUAL_API const char* vlm_get_last_error(VLMHandle handle) {
    if (!handle) {
        return "Invalid handle";
    }
    auto* vlm = static_cast<VLMVisual*>(handle);
    static std::string last_error;
    last_error = vlm->get_last_error();
    return last_error.c_str();
}

} // extern "C"