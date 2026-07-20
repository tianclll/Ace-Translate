#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/core/ConfigManager.hpp"
#include <iostream>
#include <stdexcept>
#include <windows.h>

namespace docmind {

    GlobalEngineContext& GlobalEngineContext::getInstance() {
        static GlobalEngineContext instance;
        return instance;
    }

    // ==================== 内部辅助：获取 base 目录 ====================
    static std::string getBaseDir() {
        return ConfigManager::getInstance().getNestedJson("defaults").value("base_dir", "");
    }

    // ==================== initialize ====================
    void GlobalEngineContext::initialize(const std::string& base_dir) {
        std::call_once(init_flag_, [this, base_dir]() {
            // 加载配置
            auto& config = ConfigManager::getInstance();
            config.load(base_dir);

            // 获取路径（以 base_dir 为前缀）
            std::string base = base_dir.empty() ? "." : base_dir;
            base_dir_ = base;
            auto dlls = config.getJson("dlls");
            auto models = config.getJson("models");

            // 读取 enable_* 开关（决定哪些引擎在启动时加载）
            auto defaults = ConfigManager::getInstance().getNestedJson("defaults");
            bool enable_layout = defaults.value("enable_layout", nlohmann::json(false)).get<bool>();
            bool enable_ocr = defaults.value("enable_ocr", nlohmann::json(false)).get<bool>();
            bool enable_vlm = defaults.value("enable_vlm", nlohmann::json(false)).get<bool>();
            bool enable_translator = defaults.value("enable_translator", nlohmann::json(true)).get<bool>();
            bool enable_docproc = defaults.value("enable_docproc", nlohmann::json(false)).get<bool>();
            bool enable_asr = defaults.value("enable_asr", nlohmann::json(false)).get<bool>();
            std::cout << "[DBG] enable_translator=" << enable_translator << std::endl;

            // 创建 DLLLoader
            dll_loader_ = std::make_unique<DLLLoader>();

            // 加载启用的引擎 DLL
            if (enable_layout) {
                if (!dll_loader_->loadLayoutDLL(base + "\\" + dlls.value("layout", "doclayout.dll")))
                    throw std::runtime_error("Failed to load Layout DLL");
            }
            if (enable_ocr) {
                if (!dll_loader_->loadOCRDLL(base + "\\" + dlls.value("ocr", "ocr.dll")))
                    throw std::runtime_error("Failed to load OCR DLL");
            }
            if (enable_vlm) {
                if (!dll_loader_->loadVLMDLL(base + "\\" + dlls.value("vlm", "vlm_visual.dll")))
                    std::cerr << "Warning: VLM DLL not loaded." << std::endl;
            }
            if (enable_translator) {
                if (!dll_loader_->loadTranslatorDLL(base + "\\" + dlls.value("translator", "translator.dll")))
                    std::cerr << "Warning: Translator DLL not loaded." << std::endl;
            }

            // —— 创建启用的引擎实例 ——
            if (enable_layout) ensureLayoutDetector();
            if (enable_ocr) ensureOCREngine();
            if (enable_vlm) ensureVLMEngine();
            if (enable_translator) ensureTranslatorEngine();
            if (enable_docproc) ensureDocProc();
            if (enable_asr) ensureASREngine();

            initialized_ = true;
            std::cout << "GlobalEngineContext initialized." << std::endl;
        });
    }

    // ==================== 懒加载实现 ====================

    bool GlobalEngineContext::ensureOCREngine() {
        if (ocr_) return true;
        if (!dll_loader_) { std::cerr << "GlobalEngineContext not initialized.\n"; return false; }

        auto& config = ConfigManager::getInstance();
        auto dlls = config.getJson("dlls");
        auto defaults = config.getNestedJson("defaults");

        // 懒加载 DLL
        if (!dll_loader_->ocrLoaded()) {
            std::string dll_path = base_dir_ + "\\" + dlls.value("ocr", "ocr.dll");
            if (!dll_loader_->loadOCRDLL(dll_path)) return false;
        }

        bool ocr_gpu = defaults.value("enable_gpu_ocr", nlohmann::json(true)).get<bool>();
        std::string ocr_size = defaults.value("ocr_model_size", "tiny");
        std::string ocr_base = base_dir_ + "\\models\\ocr\\" + ocr_size;
        try {
            ocr_ = std::make_unique<OCREngine>(*dll_loader_, ocr_base, ocr_gpu);
            std::cout << "OCR engine loaded on demand.\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "OCR on-demand init failed: " << e.what() << "\n";
            return false;
        }
    }

    bool GlobalEngineContext::ensureLayoutDetector() {
        if (layout_) return true;
        if (!dll_loader_) { std::cerr << "GlobalEngineContext not initialized.\n"; return false; }

        auto& config = ConfigManager::getInstance();
        auto dlls = config.getJson("dlls");
        auto models = config.getJson("models");
        auto defaults = config.getNestedJson("defaults");

        if (!dll_loader_->layoutLoaded()) {
            std::string dll_path = base_dir_ + "\\" + dlls.value("layout", "doclayout.dll");
            if (!dll_loader_->loadLayoutDLL(dll_path)) return false;
        }

        bool gpu = defaults.value("enable_gpu_layout", nlohmann::json(true)).get<bool>();
        int device = defaults.value("gpu_device_id", 0);
        try {
            layout_ = std::make_unique<LayoutDetector>(
                *dll_loader_,
                base_dir_ + "\\" + models.value("layout", "models/layout/pp_doclayoutv2.onnx"),
                gpu, device);
            std::cout << "LayoutDetector loaded on demand.\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Layout on-demand init failed: " << e.what() << "\n";
            return false;
        }
    }

    bool GlobalEngineContext::ensureVLMEngine() {
        if (vlm_) return true;
        if (!dll_loader_) { std::cerr << "GlobalEngineContext not initialized.\n"; return false; }

        auto& config = ConfigManager::getInstance();
        auto dlls = config.getJson("dlls");
        auto models = config.getJson("models");
        auto defaults = config.getNestedJson("defaults");

        if (!dll_loader_->vlmLoaded()) {
            std::string dll_path = base_dir_ + "\\" + dlls.value("vlm", "vlm_visual.dll");
            if (!dll_loader_->loadVLMDLL(dll_path)) return false;
        }

        bool vlm_gpu = defaults.value("enable_gpu_vlm", nlohmann::json(true)).get<bool>();
        int gpu_layers = vlm_gpu ? config.getInt("gpu_layers", 99) : 0;
        try {
            vlm_ = std::make_unique<VLMEngine>(
                *dll_loader_,
                base_dir_ + "\\" + models.value("vlm", "models/VLM/PaddleOCR-VL-1.6-GGUF.gguf"),
                base_dir_ + "\\" + models.value("vlm_mmproj", "models/VLM/PaddleOCR-VL-1.6-GGUF-mmproj.gguf"),
                config.getInt("vlm_threads", 4),
                config.getInt("vlm_context", 16384),
                gpu_layers);
            std::cout << "VLM engine loaded on demand.\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "VLM on-demand init failed: " << e.what() << "\n";
            return false;
        }
    }

    bool GlobalEngineContext::ensureTranslatorEngine() {
        if (translator_) return true;
        if (!dll_loader_) { std::cerr << "GlobalEngineContext not initialized.\n"; return false; }

        auto& config = ConfigManager::getInstance();
        auto dlls = config.getJson("dlls");
        auto models = config.getJson("models");
        auto defaults = config.getNestedJson("defaults");

        if (!dll_loader_->translatorLoaded()) {
            std::string dll_path = base_dir_ + "\\" + dlls.value("translator", "translator.dll");
            if (!dll_loader_->loadTranslatorDLL(dll_path)) return false;
        }

        bool trans_gpu = defaults.value("enable_gpu_translator", nlohmann::json(true)).get<bool>();
        int gpu_layers = trans_gpu ? config.getInt("gpu_layers", 99) : 0;
        std::string default_lang = ConfigManager::getInstance().getDefaultLanguage();
        std::string trans_model = defaults.value("trans_model", "Hy-MT2-1.8B-Q4_K_M.gguf");
        try {
            translator_ = std::make_unique<TranslatorEngine>(
                *dll_loader_,
                base_dir_ + "\\models\\translation\\" + trans_model,
                default_lang,
                gpu_layers);
            std::cout << "Translator engine loaded on demand.\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Translator on-demand init failed: " << e.what() << "\n";
            return false;
        }
    }

    bool GlobalEngineContext::ensureASREngine() {
        if (asr_) return true;
        if (!dll_loader_) { std::cerr << "GlobalEngineContext not initialized.\n"; return false; }

        auto& config = ConfigManager::getInstance();
        auto dlls = config.getJson("dlls");
        auto models = config.getJson("models");
        auto defaults = config.getNestedJson("defaults");

        if (!dll_loader_->asrLoaded()) {
            std::string dll_path = base_dir_ + "\\" + dlls.value("asr", "asr.dll");
            if (!dll_loader_->loadASRDLL(dll_path)) return false;
        }

        bool asr_gpu = defaults.value("enable_gpu_asr", nlohmann::json(true)).get<bool>();
        std::string asr_model = base_dir_ + "\\" + models.value("asr_model", "models/ASR/model_quant.onnx");
        std::string asr_tokens = base_dir_ + "\\" + models.value("asr_tokens", "models/ASR/tokens.json");
        std::string asr_mvn = base_dir_ + "\\" + models.value("asr_mvn", "models/ASR/am.mvn");
        try {
            asr_ = std::make_unique<ASREngine>(*dll_loader_, asr_model, asr_tokens, asr_mvn, asr_gpu ? 1 : 0);
            asr_handle_ = (void*)1;
            asr_loaded_ = true;
            std::cout << "ASR engine loaded on demand.\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "ASR on-demand init failed: " << e.what() << "\n";
            asr_loaded_ = false;
            asr_handle_ = nullptr;
            return false;
        }
    }

    bool GlobalEngineContext::ensureDocProc() {
        if (docproc_loaded_ && docproc_handle_) return true;
        if (!dll_loader_) { std::cerr << "GlobalEngineContext not initialized.\n"; return false; }

        auto& config = ConfigManager::getInstance();
        auto dlls = config.getJson("dlls");
        auto models = config.getJson("models");
        auto defaults = config.getNestedJson("defaults");

        std::string dll_path = base_dir_ + "\\" + dlls.value("docproc", "DocumentImageProcessor.dll");
        if (!dll_loader_->loadDocumentImageProcessorDLL(dll_path)) return false;

        bool docproc_gpu = defaults.value("enable_gpu_docproc", nlohmann::json(true)).get<bool>();
        int device = defaults.value("gpu_device_id", 0);

        docproc_handle_ = dll_loader_->docproc_create();
        if (!docproc_handle_) return false;

        std::string docproc_model = base_dir_ + "\\" + models.value("docproc", "models/uvdoc/UVDoc_grid.onnx");
        if (dll_loader_->docproc_init(docproc_handle_, docproc_model.c_str(), docproc_gpu, device)) {
            docproc_loaded_ = true;
            std::cout << "DocumentImageProcessor loaded on demand.\n";
            return true;
        }
        std::cerr << "DocumentImageProcessor init failed.\n";
        dll_loader_->docproc_release(docproc_handle_);
        docproc_handle_ = nullptr;
        return false;
    }

} // namespace docmind