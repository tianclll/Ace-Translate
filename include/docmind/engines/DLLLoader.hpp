#pragma once

#include <string>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#define DLL_HANDLE HMODULE
#define LOAD_DLL(path) LoadLibraryA(path)
#define GET_FUNC(handle, name) GetProcAddress(handle, name)
#define FREE_DLL(handle) FreeLibrary(handle)
#else
#include <dlfcn.h>
#define DLL_HANDLE void*
#define LOAD_DLL(path) dlopen(path, RTLD_LAZY)
#define GET_FUNC(handle, name) dlsym(handle, name)
#define FREE_DLL(handle) dlclose(handle)
#endif

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <iostream>

// 引入 types.hpp 避免重复定义
#include "docmind/types.hpp"

namespace docmind {

// ============================================================
// Layout 结构体（与 DLL 完全一致）
// ============================================================
#pragma pack(push, 1)
    struct LayoutBox {
        int class_id;
        float score;
        int x;
        int y;
        int width;
        int height;
        char label[64];
    };
#pragma pack(pop)

// ============================================================
// DLL 加载器
// ============================================================
    class DLLLoader {
    public:
        DLLLoader() = default;
        ~DLLLoader() { unload(); }

        bool loadLayoutDLL(const std::string& dll_path);
        bool loadOCRDLL(const std::string& dll_path);
        bool loadVLMDLL(const std::string& dll_path);
        bool loadTranslatorDLL(const std::string& dll_path);

        // ---------- Layout 函数指针 ----------
        using CreateLayoutFunc = void* (*)();
        using InitLayoutFunc = bool (*)(void*, const char*, bool, int);
        using DetectLayoutFunc = int (*)(void*, const unsigned char*, int, int, int, LayoutBox*, int);
        using DestroyLayoutFunc = void (*)(void*);

        CreateLayoutFunc createLayout = nullptr;
        InitLayoutFunc initLayout = nullptr;
        DetectLayoutFunc detectLayout = nullptr;
        DestroyLayoutFunc destroyLayout = nullptr;

        // ---------- OCR 函数指针 ----------
        using OCRCreateFunc = void* (*)(const char*, int);
        using OCRDestroyFunc = void (*)(void*);
        using OCRRecognizeBufferFunc = char* (*)(void*, const unsigned char*, int, int, int);
        using OCRRecognizeCropFunc = char* (*)(void*, const unsigned char*, int, int, int);
        using OCRDetectorCreateFunc = void* (*)(const char*, int);
        using OCRDetectorDestroyFunc = void (*)(void*);
        using OCRDetectorDetectBufferFunc = char* (*)(void*, const unsigned char*, int, int, int);
        using OCRFreeStringFunc = void (*)(char*);

        OCRCreateFunc ocrCreate = nullptr;
        OCRDestroyFunc ocrDestroy = nullptr;
        OCRRecognizeBufferFunc ocrRecognizeBuffer = nullptr;
        OCRRecognizeCropFunc ocrRecognizeCrop = nullptr;
        OCRDetectorCreateFunc ocrDetectorCreate = nullptr;
        OCRDetectorDestroyFunc ocrDetectorDestroy = nullptr;
        OCRDetectorDetectBufferFunc ocrDetectorDetectBuffer = nullptr;
        OCRFreeStringFunc ocrFreeString = nullptr;

        // ---------- VLM 函数指针 ----------
        using VLMCreateFunc = void* (*)(const char*, const char*, int, int, int);
        using VLMDestroyFunc = void (*)(void*);
        using VLMUnderstandFromMemoryFunc = const char* (*)(void*, const unsigned char*, size_t, const char*, int);
        using VLMFreeStringFunc = void (*)(const char*);
        using VLMGetLastErrorFunc = const char* (*)(void*);

        VLMCreateFunc vlm_create = nullptr;
        VLMDestroyFunc vlm_destroy = nullptr;
        VLMUnderstandFromMemoryFunc vlm_understand_from_memory = nullptr;
        VLMFreeStringFunc vlm_free_string = nullptr;
        VLMGetLastErrorFunc vlm_get_last_error = nullptr;

        // ---------- Translator 函数指针 ----------
        using TranslatorCreateFunc = void* (*)(const char*, int);
        using TranslatorDestroyFunc = void (*)(void*);
        using TranslatorTranslateFunc = const char* (*)(void*, const char*, const char*, int);
        using TranslatorFreeStringFunc = void (*)(const char*);

        TranslatorCreateFunc translator_create = nullptr;
        TranslatorDestroyFunc translator_destroy = nullptr;
        TranslatorTranslateFunc translator_translate = nullptr;
        TranslatorFreeStringFunc translator_free_string = nullptr;

        // ---------- UVDoc 函数指针 ----------
        using DocProcCreateFunc = void* (*)();
        using DocProcInitFunc = bool (*)(void*, const char*, bool, int);
        using DocProcProcessFunc = bool (*)(void*, unsigned char*, int, int, int, bool, bool, unsigned char**, int*, int*, int*);
        using DocProcReleaseFunc = void (*)(void*);
        using DocProcFreeOutputFunc = void (*)(unsigned char*);

        DocProcCreateFunc docproc_create = nullptr;
        DocProcInitFunc docproc_init = nullptr;
        DocProcProcessFunc docproc_process = nullptr;
        DocProcReleaseFunc docproc_release = nullptr;
        DocProcFreeOutputFunc docproc_free_output = nullptr;

        // ---------- ASR 函数指针 ----------
        using ASRCreateFunc = void* (*)(const char*, const char*, const char*, int);
        using ASRDestroyFunc = void (*)(void*);
        using ASRRecognizeFunc = char* (*)(void*, const short*, int);

        ASRCreateFunc asr_create = nullptr;
        ASRDestroyFunc asr_destroy = nullptr;
        ASRRecognizeFunc asr_recognize = nullptr;

        bool loadASRDLL(const std::string& dll_path);

        bool layoutLoaded() const { return layout_handle_ != nullptr; }
        bool ocrLoaded() const { return ocr_handle_ != nullptr; }
        bool vlmLoaded() const { return vlm_handle_ != nullptr; }
        bool translatorLoaded() const { return translator_handle_ != nullptr; }
        bool loadDocumentImageProcessorDLL(const std::string& dll_path);
        bool docprocLoaded() const { return docproc_handle_ != nullptr; }
        bool asrLoaded() const { return asr_handle_ != nullptr; }
    private:
        void unload();
        DLL_HANDLE layout_handle_ = nullptr;
        DLL_HANDLE ocr_handle_ = nullptr;
        DLL_HANDLE vlm_handle_ = nullptr;
        DLL_HANDLE translator_handle_ = nullptr;
        DLL_HANDLE docproc_handle_ = nullptr;
        DLL_HANDLE asr_handle_ = nullptr;
    };

} // namespace docmind