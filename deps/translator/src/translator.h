#pragma once

#include <string>
#include <memory>

#ifdef _WIN32
#ifdef TRANSLATOR_EXPORTS
#define TRANSLATOR_API __declspec(dllexport)
#else
#define TRANSLATOR_API __declspec(dllimport)
#endif
#else
#define TRANSLATOR_API
#endif

// ============================================================
// C++ 接口
// ============================================================

class TRANSLATOR_API Translator {
public:
    Translator(
            const std::string& model_path,
            int n_gpu_layers = 0
    );

    ~Translator();

    std::string translate(
            const std::string& source_text,
            const std::string& target_language,
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

typedef void* TranslatorHandle;

TRANSLATOR_API TranslatorHandle translator_create(
        const char* model_path,
        int n_gpu_layers
);

TRANSLATOR_API void translator_destroy(
        TranslatorHandle handle
);

TRANSLATOR_API const char* translator_translate(
        TranslatorHandle handle,
        const char* source_text,
        const char* target_language,
        int max_tokens
);

TRANSLATOR_API void translator_free_string(
        const char* str
);
}