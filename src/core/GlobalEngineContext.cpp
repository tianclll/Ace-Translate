#include "docmind/core/GlobalEngineContext.hpp"
#include "docmind/core/ConfigManager.hpp"
#include <iostream>
#include <stdexcept>

namespace docmind {

    GlobalEngineContext& GlobalEngineContext::getInstance() {
        static GlobalEngineContext instance;
        return instance;
    }

    void GlobalEngineContext::initialize(const std::string& base_dir) {
        std::call_once(init_flag_, [this, base_dir]() {
            // 加载配置
            auto& config = ConfigManager::getInstance();
            config.load(base_dir);

            // 获取路径（以 base_dir 为前缀）
            std::string base = base_dir.empty() ? "." : base_dir;
            auto dlls = config.getJson("dlls");
            auto models = config.getJson("models");

            // 加载 DLL
            dll_loader_ = std::make_unique<DLLLoader>();
            if (!dll_loader_->loadLayoutDLL(base + "\\" + dlls.value("layout", "doclayout.dll")))
                throw std::runtime_error("Failed to load Layout DLL");
            if (!dll_loader_->loadOCRDLL(base + "\\" + dlls.value("ocr", "ocr.dll")))
                throw std::runtime_error("Failed to load OCR DLL");
            if (!dll_loader_->loadVLMDLL(base + "\\" + dlls.value("vlm", "vlm_visual.dll")))
                std::cerr << "Warning: VLM DLL not loaded." << std::endl;
            if (!dll_loader_->loadTranslatorDLL(base + "\\" + dlls.value("translator", "translator.dll")))
                std::cerr << "Warning: Translator DLL not loaded." << std::endl;

            // GPU 开关：从 defaults 读取（放在最前面，所有引擎都能用）
            bool ocr_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_ocr", true);
            bool vlm_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_vlm", true);
            bool trans_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_translator", true);
            bool layout_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_layout", true);
            bool docproc_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_docproc", true);
            int docproc_device = ConfigManager::getInstance().getNestedJson("defaults").value("gpu_device_id", 0);
            int vlm_gpu_layers = vlm_gpu ? config.getInt("gpu_layers", 99) : 0;
            int trans_gpu_layers = trans_gpu ? config.getInt("gpu_layers", 99) : 0;

            // 加载 DocumentImageProcessor（可选）
            std::string docproc_dll = base + "\\" + dlls.value("docproc", "DocumentImageProcessor.dll");
            if (dll_loader_->loadDocumentImageProcessorDLL(docproc_dll)) {
                docproc_handle_ = dll_loader_->docproc_create();
                if (docproc_handle_) {
                    std::string docproc_model = base + "\\" + models.value("docproc", "models/uvdoc/UVDoc_grid.onnx");
                    if (dll_loader_->docproc_init(docproc_handle_, docproc_model.c_str(), docproc_gpu, docproc_device)) {
                        docproc_loaded_ = true;
                        std::cout << "DocumentImageProcessor initialized." << std::endl;
                    } else {
                        std::cerr << "DocumentImageProcessor init failed." << std::endl;
                        dll_loader_->docproc_release(docproc_handle_);
                        docproc_handle_ = nullptr;
                    }
                }
            } else {
                std::cerr << "Warning: DocumentImageProcessor DLL not loaded." << std::endl;
            }

            // 创建引擎
            layout_ = std::make_unique<LayoutDetector>(*dll_loader_, base + "\\" + models.value("layout", "models/layout/pp_doclayoutv2.onnx"), docproc_gpu, docproc_device);
            ocr_ = std::make_unique<OCREngine>(*dll_loader_, base + "\\" + models.value("ocr_dir", "models"), ocr_gpu);

            if (dll_loader_->vlmLoaded()) {
                vlm_ = std::make_unique<VLMEngine>(
                        *dll_loader_,
                        base + "\\" + models.value("vlm", "models/VLM/PaddleOCR-VL-1.6-GGUF.gguf"),
                        base + "\\" + models.value("vlm_mmproj", "models/VLM/PaddleOCR-VL-1.6-GGUF-mmproj.gguf"),
                        config.getInt("vlm_threads", 4),
                        config.getInt("vlm_context", 16384),
                        vlm_gpu_layers
                );
            }

            if (dll_loader_->translatorLoaded()) {
                std::string default_lang = ConfigManager::getInstance().getDefaultLanguage();
                translator_ = std::make_unique<TranslatorEngine>(
                        *dll_loader_,
                        base + "\\" + models.value("translator", "models/translation/Hy-MT2-1.8B-Q4_K_M.gguf"),
                        default_lang,  // 默认语言，实际翻译时动态指定
                        trans_gpu_layers
                );
            }

            initialized_ = true;
            std::cout << "GlobalEngineContext initialized." << std::endl;
        });
    }

} // namespace docmind