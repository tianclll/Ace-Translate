#include "docmind/engines/DLLLoader.hpp"
#include <iostream>

namespace docmind {

    bool DLLLoader::loadLayoutDLL(const std::string& dll_path) {
        layout_handle_ = LOAD_DLL(dll_path.c_str());
        if (!layout_handle_) {
            std::cerr << "Failed to load layout DLL: " << dll_path << std::endl;
            return false;
        }

        createLayout = (CreateLayoutFunc)GET_FUNC(layout_handle_, "CreatePPDocLayout");
        initLayout = (InitLayoutFunc)GET_FUNC(layout_handle_, "InitPPDocLayout");
        detectLayout = (DetectLayoutFunc)GET_FUNC(layout_handle_, "DetectLayout");
        destroyLayout = (DestroyLayoutFunc)GET_FUNC(layout_handle_, "DestroyPPDocLayout");

        if (!createLayout || !initLayout || !detectLayout || !destroyLayout) {
            std::cerr << "Failed to get layout functions" << std::endl;
            return false;
        }
        std::cout << "Layout DLL loaded successfully" << std::endl;
        return true;
    }

    bool DLLLoader::loadOCRDLL(const std::string& dll_path) {
        ocr_handle_ = LOAD_DLL(dll_path.c_str());
        if (!ocr_handle_) {
            std::cerr << "Failed to load OCR DLL: " << dll_path << std::endl;
            return false;
        }

        ocrCreate = (OCRCreateFunc)GET_FUNC(ocr_handle_, "ocr_create");
        ocrDestroy = (OCRDestroyFunc)GET_FUNC(ocr_handle_, "ocr_destroy");
        ocrRecognizeBuffer = (OCRRecognizeBufferFunc)GET_FUNC(ocr_handle_, "ocr_recognize_buffer");
        ocrRecognizeCrop = (OCRRecognizeCropFunc)GET_FUNC(ocr_handle_, "ocr_recognize_crop");
        ocrDetectorCreate = (OCRDetectorCreateFunc)GET_FUNC(ocr_handle_, "ocr_detector_create");
        ocrDetectorDestroy = (OCRDetectorDestroyFunc)GET_FUNC(ocr_handle_, "ocr_detector_destroy");
        ocrDetectorDetectBuffer = (OCRDetectorDetectBufferFunc)GET_FUNC(ocr_handle_, "ocr_detector_detect_buffer");
        ocrFreeString = (OCRFreeStringFunc)GET_FUNC(ocr_handle_, "ocr_free_string");

        if (!ocrCreate || !ocrDestroy || !ocrRecognizeBuffer || !ocrFreeString ||
            !ocrDetectorCreate || !ocrDetectorDestroy || !ocrDetectorDetectBuffer) {
            std::cerr << "Failed to get OCR functions" << std::endl;
            return false;
        }

        // 检查 ocr_recognize_crop 是否可选（可能不存在，但最好有）
        if (!ocrRecognizeCrop) {
            std::cout << "Warning: ocr_recognize_crop not found in DLL, using fallback" << std::endl;
            // 可以 fallback 到 ocr_recognize_buffer
            ocrRecognizeCrop = ocrRecognizeBuffer;
        }

        std::cout << "OCR DLL loaded successfully" << std::endl;
        return true;
    }
    bool DLLLoader::loadVLMDLL(const std::string& dll_path) {
        vlm_handle_ = LOAD_DLL(dll_path.c_str());
        if (!vlm_handle_) {
            std::cerr << "Failed to load VLM DLL: " << dll_path << std::endl;
            return false;
        }

        vlm_create = (VLMCreateFunc)GET_FUNC(vlm_handle_, "vlm_create");
        vlm_destroy = (VLMDestroyFunc)GET_FUNC(vlm_handle_, "vlm_destroy");
        vlm_understand_from_memory = (VLMUnderstandFromMemoryFunc)GET_FUNC(vlm_handle_, "vlm_understand_from_memory");
        vlm_free_string = (VLMFreeStringFunc)GET_FUNC(vlm_handle_, "vlm_free_string");
        vlm_get_last_error = (VLMGetLastErrorFunc)GET_FUNC(vlm_handle_, "vlm_get_last_error");

        if (!vlm_create || !vlm_destroy || !vlm_understand_from_memory || !vlm_free_string) {
            std::cerr << "Failed to get VLM functions from DLL" << std::endl;
            unload();
            return false;
        }

        std::cout << "VLM DLL loaded successfully" << std::endl;
        return true;
    }
    bool DLLLoader::loadTranslatorDLL(const std::string& dll_path) {
        translator_handle_ = LOAD_DLL(dll_path.c_str());
        if (!translator_handle_) {
            std::cerr << "Failed to load Translator DLL: " << dll_path << std::endl;
            return false;
        }

        translator_create = (TranslatorCreateFunc)GET_FUNC(translator_handle_, "translator_create");
        translator_destroy = (TranslatorDestroyFunc)GET_FUNC(translator_handle_, "translator_destroy");
        translator_translate = (TranslatorTranslateFunc)GET_FUNC(translator_handle_, "translator_translate");
        translator_free_string = (TranslatorFreeStringFunc)GET_FUNC(translator_handle_, "translator_free_string");

        if (!translator_create || !translator_destroy || !translator_translate || !translator_free_string) {
            std::cerr << "Failed to get Translator functions from DLL" << std::endl;
            unload();
            return false;
        }

        std::cout << "Translator DLL loaded successfully" << std::endl;
        return true;
    }
    bool DLLLoader::loadDocumentImageProcessorDLL(const std::string& dll_path) {
        docproc_handle_ = LOAD_DLL(dll_path.c_str());
        if (!docproc_handle_) {
            std::cerr << "Failed to load DocumentImageProcessor DLL: " << dll_path << std::endl;
            return false;
        }

        docproc_create = (DocProcCreateFunc)GET_FUNC(docproc_handle_, "CreateProcessor");
        docproc_init = (DocProcInitFunc)GET_FUNC(docproc_handle_, "InitProcessor");
        docproc_process = (DocProcProcessFunc)GET_FUNC(docproc_handle_, "ProcessImage");
        docproc_release = (DocProcReleaseFunc)GET_FUNC(docproc_handle_, "ReleaseProcessor");
        docproc_free_output = (DocProcFreeOutputFunc)GET_FUNC(docproc_handle_, "FreeOutput");

        if (!docproc_create || !docproc_init || !docproc_process || !docproc_release || !docproc_free_output) {
            std::cerr << "Failed to get DocumentImageProcessor functions" << std::endl;
            unload();
            return false;
        }

        std::cout << "DocumentImageProcessor DLL loaded successfully" << std::endl;
        return true;
    }
    void DLLLoader::unload() {
        if (layout_handle_) {
            FREE_DLL(layout_handle_);
            layout_handle_ = nullptr;
        }
        if (ocr_handle_) {
            FREE_DLL(ocr_handle_);
            ocr_handle_ = nullptr;
        }
        if (vlm_handle_) {
            FREE_DLL(vlm_handle_);
            vlm_handle_ = nullptr;
        }
        if (translator_handle_) {
            FREE_DLL(translator_handle_);
            translator_handle_ = nullptr;
        }
        if (docproc_handle_) {
            FREE_DLL(docproc_handle_);
            docproc_handle_ = nullptr;
        }
    }

} // namespace formula