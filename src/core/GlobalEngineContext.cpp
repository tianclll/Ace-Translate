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

    void GlobalEngineContext::initialize(const std::string& base_dir) {
        std::call_once(init_flag_, [this, base_dir]() {
            // 加载配置
            auto& config = ConfigManager::getInstance();
            config.load(base_dir);

            // 获取路径（以 base_dir 为前缀）
            std::string base = base_dir.empty() ? "." : base_dir;
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

            // 加载 DLL（仅加载启用的引擎 DLL）
            dll_loader_ = std::make_unique<DLLLoader>();
            if (enable_layout) {
                if (!dll_loader_->loadLayoutDLL(base + "\\" + dlls.value("layout", "doclayout.dll")))
                    throw std::runtime_error("Failed to load Layout DLL");
            } else {
                std::cout << "Layout engine disabled by config." << std::endl;
            }
            if (enable_ocr) {
                if (!dll_loader_->loadOCRDLL(base + "\\" + dlls.value("ocr", "ocr.dll")))
                    throw std::runtime_error("Failed to load OCR DLL");
            } else {
                std::cout << "OCR engine disabled by config." << std::endl;
            }
            if (enable_vlm) {
                if (!dll_loader_->loadVLMDLL(base + "\\" + dlls.value("vlm", "vlm_visual.dll")))
                    std::cerr << "Warning: VLM DLL not loaded." << std::endl;
            } else {
                std::cout << "VLM engine disabled by config." << std::endl;
            }
            if (enable_translator) {
                if (!dll_loader_->loadTranslatorDLL(base + "\\" + dlls.value("translator", "translator.dll")))
                    std::cerr << "Warning: Translator DLL not loaded." << std::endl;
            } else {
                std::cout << "Translator engine disabled by config." << std::endl;
            }

            // GPU 开关：从 defaults 读取（放在最前面，所有引擎都能用）
            bool ocr_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_ocr", nlohmann::json(true)).get<bool>();
            bool vlm_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_vlm", nlohmann::json(true)).get<bool>();
            bool trans_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_translator", nlohmann::json(true)).get<bool>();
            bool layout_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_layout", nlohmann::json(true)).get<bool>();
            bool docproc_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_docproc", nlohmann::json(true)).get<bool>();
            bool asr_gpu = ConfigManager::getInstance().getNestedJson("defaults").value("enable_gpu_asr", nlohmann::json(true)).get<bool>();
            int docproc_device = ConfigManager::getInstance().getNestedJson("defaults").value("gpu_device_id", 0);
            int vlm_gpu_layers = vlm_gpu ? config.getInt("gpu_layers", 99) : 0;
            int trans_gpu_layers = trans_gpu ? config.getInt("gpu_layers", 99) : 0;

            // 加载 DocumentImageProcessor（可选，受 enable_docproc 控制）
            if (enable_docproc) {
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
            } else {
                std::cout << "DocumentImageProcessor disabled by config." << std::endl;
            }

            // 创建引擎（受 enable_* 控制）
            if (enable_layout) {
                layout_ = std::make_unique<LayoutDetector>(*dll_loader_, base + "\\" + models.value("layout", "models/layout/pp_doclayoutv2.onnx"), docproc_gpu, docproc_device);
            } else {
                std::cout << "LayoutDetector disabled by config." << std::endl;
            }

            if (enable_ocr) {
                // OCR：根据模型大小拼接路径
                std::string ocr_size = ConfigManager::getInstance().getNestedJson("defaults").value("ocr_model_size", "tiny");
                std::string ocr_base = base + "\\models\\ocr\\" + ocr_size;
                ocr_ = std::make_unique<OCREngine>(*dll_loader_, ocr_base, ocr_gpu);
            } else {
                std::cout << "OCR engine disabled by config." << std::endl;
            }

            if (enable_vlm && dll_loader_->vlmLoaded()) {
                vlm_ = std::make_unique<VLMEngine>(
                        *dll_loader_,
                        base + "\\" + models.value("vlm", "models/VLM/PaddleOCR-VL-1.6-GGUF.gguf"),
                        base + "\\" + models.value("vlm_mmproj", "models/VLM/PaddleOCR-VL-1.6-GGUF-mmproj.gguf"),
                        config.getInt("vlm_threads", 4),
                        config.getInt("vlm_context", 16384),
                        vlm_gpu_layers
                );
            } else if (!enable_vlm) {
                std::cout << "VLM engine disabled by config." << std::endl;
            }

            if (dll_loader_->translatorLoaded()) {
                std::string default_lang = ConfigManager::getInstance().getDefaultLanguage();
                std::string trans_model = ConfigManager::getInstance().getNestedJson("defaults").value("trans_model", "Hy-MT2-1.8B-Q4_K_M.gguf");
                translator_ = std::make_unique<TranslatorEngine>(
                        *dll_loader_,
                        base + "\\models\\translation\\" + trans_model,
                        default_lang,
                        trans_gpu_layers
                );
            }

            // 加载 ASR 引擎（受 enable_asr 控制）
            if (enable_asr) {
                std::string asr_dll_path = base + "\\" + dlls.value("asr", "asr.dll");
                if (dll_loader_->loadASRDLL(asr_dll_path)) {
                    std::string asr_model = base + "\\" + models.value("asr_model", "models/ASR/model_quant.onnx");
                    std::string asr_tokens = base + "\\" + models.value("asr_tokens", "models/ASR/tokens.json");
                    std::string asr_mvn = base + "\\" + models.value("asr_mvn", "models/ASR/am.mvn");
                    try {
                        asr_ = std::make_unique<ASREngine>(*dll_loader_, asr_model, asr_tokens, asr_mvn, asr_gpu ? 1 : 0);
                        asr_handle_ = asr_->isLoaded() ? (void*)1 : nullptr;
                        asr_loaded_ = asr_->isLoaded();
                    } catch (const std::exception& e) {
                        std::cerr << "ASREngine init failed: " << e.what() << std::endl;
                        asr_loaded_ = false;
                        asr_handle_ = nullptr;
                    }
                } else {
                    std::cerr << "Warning: ASR DLL not loaded." << std::endl;
                }
            } else {
                std::cout << "ASR engine disabled by config." << std::endl;
            }

            initialized_ = true;
            std::cout << "GlobalEngineContext initialized." << std::endl;
        });
    }

} // namespace docmind