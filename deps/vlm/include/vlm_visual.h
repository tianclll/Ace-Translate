#pragma once

#include <string>
#include <memory>

#ifdef _WIN32
#ifdef VLM_VISUAL_EXPORTS
#define VLM_VISUAL_API __declspec(dllexport)
#else
#define VLM_VISUAL_API __declspec(dllimport)
#endif
#else
#define VLM_VISUAL_API
#endif

// ============================================================
// C++ 接口
// ============================================================

class VLM_VISUAL_API VLMVisual {
public:
    /**
     * @brief 构造函数
     * @param model_path LLM模型路径 (.gguf)
     * @param mmproj_path 多模态投影模型路径 (.mmproj)
     * @param n_threads CPU线程数
     * @param n_ctx 上下文长度
     * @param n_gpu_layers GPU层数 (0=CPU, 999=全部GPU)
     */
    VLMVisual(
            const std::string& model_path,
            const std::string& mmproj_path = "",
            int n_threads = 4,
            int n_ctx = 8192,
            int n_gpu_layers = 0
    );

    ~VLMVisual();

    /**
     * @brief 理解图像内容
     * @param image_path 图像路径
     * @param prompt 提示词
     * @param max_tokens 最大生成token数
     * @return 描述文本
     */
    std::string understand(
            const std::string& image_path,
            const std::string& prompt,
            int max_tokens = 2048
    );

    /**
     * @brief 从内存数据理解图像
     * @param image_data 图像数据
     * @param data_size 数据大小
     * @param prompt 提示词
     * @param max_tokens 最大生成token数
     * @return 描述文本
     */
    std::string understand_from_memory(
            const unsigned char* image_data,
            size_t data_size,
            const std::string& prompt,
            int max_tokens = 2048
    );

    /**
     * @brief 获取最后一个错误信息
     */
    std::string get_last_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================
// C 接口
// ============================================================

extern "C" {
typedef void* VLMHandle;

/**
 * @brief 创建 VLM 实例
 * @param model_path LLM模型路径
 * @param mmproj_path 多模态投影模型路径
 * @param n_threads CPU线程数
 * @param n_ctx 上下文长度
 * @param n_gpu_layers GPU层数 (0=CPU, 999=全部GPU)
 */
VLM_VISUAL_API VLMHandle vlm_create(
        const char* model_path,
        const char* mmproj_path,
        int n_threads,
        int n_ctx,
        int n_gpu_layers
);

VLM_VISUAL_API void vlm_destroy(VLMHandle handle);

VLM_VISUAL_API const char* vlm_understand(
        VLMHandle handle,
        const char* image_path,
        const char* prompt,
        int max_tokens
);

VLM_VISUAL_API const char* vlm_understand_from_memory(
        VLMHandle handle,
        const unsigned char* image_data,
        size_t data_size,
        const char* prompt,
        int max_tokens
);

VLM_VISUAL_API void vlm_free_string(const char* str);

VLM_VISUAL_API const char* vlm_get_last_error(VLMHandle handle);
}