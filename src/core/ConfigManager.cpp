#include "docmind/core/ConfigManager.hpp"
#include <fstream>
#include <iostream>
#include <windows.h>

namespace docmind {

    static nlohmann::json defaultConfig() {
        return {
            {"dlls", {
                {"layout", "doclayout.dll"},
                {"ocr", "ocr.dll"},
                {"vlm", "vlm_visual.dll"},
                {"translator", "translator.dll"},
                {"docproc", "DocumentImageProcessor.dll"}
            }},
            {"models", {
                {"layout", "models/layout/pp_doclayoutv2.onnx"},
                {"ocr_dir", "models"},
                {"vlm", "models/VLM/PaddleOCR-VL-1.6-GGUF.gguf"},
                {"vlm_mmproj", "models/VLM/PaddleOCR-VL-1.6-GGUF-mmproj.gguf"},
                {"translator", "models/translation/Hy-MT2-1.8B-Q4_K_M.gguf"},
                {"docproc", "models/uvdoc/UVDoc_grid.onnx"}
            }},
            {"defaults", {
                {"target_language", "English"},
                {"layout_threshold", 0.5f},
                {"pdf_dpi", 200},
                {"vlm_threads", 4},
                {"vlm_context", 16384},
                {"gpu_layers", 99},
                {"enable_warp", true},
                {"enable_enhance", true},
                {"enable_gpu_vlm", true},
                {"enable_gpu_translator", true},
                {"enable_gpu_ocr", true},
                {"enable_gpu_layout", true},
                {"enable_gpu_docproc", true},
                {"gpu_device_id", 0},
            }}
        };
    }

    static std::string get_exe_directory() {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exeDir(exePath);
        size_t pos = exeDir.find_last_of("\\/");
        if (pos != std::string::npos)
            exeDir = exeDir.substr(0, pos);
        return exeDir;
    }

    ConfigManager& ConfigManager::getInstance() {
        static ConfigManager instance;
        return instance;
    }

    void ConfigManager::load(const std::string& base_dir) {
        if (loaded_) return;

        std::string config_path = base_dir.empty() ? get_exe_directory() : base_dir;
        config_path += "\\config.json";

        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Warning: config.json not found, using defaults." << std::endl;
            // 使用默认配置（硬编码）
            config_ = defaultConfig();
        } else {
            try {
                file >> config_;
                std::cout << "Config loaded from " << config_path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse config.json: " << e.what() << std::endl;
                // 回退默认配置
                config_ = defaultConfig();
            }
            file.close();
        }
        loaded_ = true;
    }

    std::string ConfigManager::getString(const std::string& key, const std::string& default_val) const {
        if (!loaded_) return default_val;
        try {
            return config_.value(key, default_val);
        } catch (...) {
            return default_val;
        }
    }

    int ConfigManager::getInt(const std::string& key, int default_val) const {
        if (!loaded_) return default_val;
        try {
            return config_.value(key, default_val);
        } catch (...) {
            return default_val;
        }
    }

    float ConfigManager::getFloat(const std::string& key, float default_val) const {
        if (!loaded_) return default_val;
        try {
            return config_.value(key, default_val);
        } catch (...) {
            return default_val;
        }
    }

    bool ConfigManager::getBool(const std::string& key, bool default_val) const {
        if (!loaded_) return default_val;
        try {
            return config_.value(key, default_val);
        } catch (...) {
            return default_val;
        }
    }
    std::string ConfigManager::getDefaultLanguage() const {
        return getString("defaults.target_language", "English");
    }

    void ConfigManager::setString(const std::string& key, const std::string& value) {
        config_[key] = value;
    }

    void ConfigManager::setInt(const std::string& key, int value) {
        config_[key] = value;
    }

    void ConfigManager::setBool(const std::string& key, bool value) {
        config_[key] = value;
    }

    void ConfigManager::save(const std::string& base_dir) {
        std::string path = base_dir.empty() ? get_exe_directory() : base_dir;
        path += "\\config.json";
        std::ofstream file(path);
        if (file.is_open()) {
            file << config_.dump(4);
            file.close();
        }
    }

    nlohmann::json ConfigManager::getJson(const std::string& key) const {
        if (!loaded_) return nlohmann::json::object();
        try {
            return config_.value(key, nlohmann::json::object());
        } catch (...) {
            return nlohmann::json::object();
        }
    }

    // 点号分隔的嵌套 JSON 访问
    std::string ConfigManager::getNestedString(const std::string& key_path, const std::string& default_val) const {
        if (!loaded_) return default_val;
        try {
            const nlohmann::json* obj = &config_;
            size_t start = 0, end;
            while ((end = key_path.find('.', start)) != std::string::npos) {
                std::string seg = key_path.substr(start, end - start);
                if (!obj->contains(seg)) return default_val;
                obj = &(*obj)[seg];
                start = end + 1;
            }
            std::string seg = key_path.substr(start);
            if (!obj->contains(seg)) return default_val;
            auto& val = (*obj)[seg];
            if (val.is_string()) return val.get<std::string>();
            return val.dump();
        } catch (...) {
            return default_val;
        }
    }

    void ConfigManager::setNestedString(const std::string& key_path, const std::string& value) {
        try {
            nlohmann::json* obj = &config_;
            size_t start = 0, end;
            while ((end = key_path.find('.', start)) != std::string::npos) {
                std::string seg = key_path.substr(start, end - start);
                if (!obj->contains(seg) || !(*obj)[seg].is_object())
                    (*obj)[seg] = nlohmann::json::object();
                obj = &(*obj)[seg];
                start = end + 1;
            }
            std::string seg = key_path.substr(start);
            (*obj)[seg] = value;
        } catch (...) {}
    }

    nlohmann::json ConfigManager::getNestedJson(const std::string& key_path) const {
        if (!loaded_) return nlohmann::json::object();
        try {
            const nlohmann::json* obj = &config_;
            size_t start = 0, end;
            while ((end = key_path.find('.', start)) != std::string::npos) {
                std::string seg = key_path.substr(start, end - start);
                if (!obj->contains(seg)) return nlohmann::json::object();
                obj = &(*obj)[seg];
                start = end + 1;
            }
            std::string seg = key_path.substr(start);
            if (!obj->contains(seg)) return nlohmann::json::object();
            return (*obj)[seg];
        } catch (...) {
            return nlohmann::json::object();
        }
    }

    void ConfigManager::setNestedBool(const std::string& key_path, bool value) {
        try {
            nlohmann::json* obj = &config_;
            size_t start = 0, end;
            while ((end = key_path.find('.', start)) != std::string::npos) {
                std::string seg = key_path.substr(start, end - start);
                if (!obj->contains(seg) || !(*obj)[seg].is_object())
                    (*obj)[seg] = nlohmann::json::object();
                obj = &(*obj)[seg];
                start = end + 1;
            }
            std::string seg = key_path.substr(start);
            (*obj)[seg] = value;
        } catch (...) {}
    }

} // namespace docmind