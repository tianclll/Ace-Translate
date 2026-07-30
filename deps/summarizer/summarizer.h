#pragma once

#include <string>
#include <memory>

#ifdef _WIN32
#ifdef SUMMARIZER_EXPORTS
#define SUMMARIZER_API __declspec(dllexport)
#else
#define SUMMARIZER_API __declspec(dllimport)
#endif
#else
#define SUMMARIZER_API
#endif

// ============================================================
// C++ 接口
// ============================================================

class SUMMARIZER_API Summarizer {
public:
    Summarizer(
            const std::string& model_path,
            int n_gpu_layers = 0
    );

    ~Summarizer();

    /// 生成中文摘要
    std::string summarize(
            const std::string& source_text,
            int max_tokens
    );

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================
// C 接口
// ============================================================

extern "C" {

typedef void* SummarizerHandle;

SUMMARIZER_API SummarizerHandle summarizer_create(
        const char* model_path,
        int n_gpu_layers
);

SUMMARIZER_API void summarizer_destroy(
        SummarizerHandle handle
);

SUMMARIZER_API const char* summarizer_summarize(
        SummarizerHandle handle,
        const char* source_text,
        int max_tokens
);

SUMMARIZER_API void summarizer_free_string(
        const char* str
);

}
