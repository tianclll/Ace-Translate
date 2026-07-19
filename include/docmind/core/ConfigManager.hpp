#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace docmind {

    class ConfigManager {
    public:
        static ConfigManager& getInstance();

        // 加载配置文件（默认在 base_dir/config.json，若不存在则使用默认值）
        void load(const std::string& base_dir = "");

        // 获取/设置配置项
        std::string getString(const std::string& key, const std::string& default_val = "") const;
        int getInt(const std::string& key, int default_val = 0) const;
        float getFloat(const std::string& key, float default_val = 0.0f) const;
        bool getBool(const std::string& key, bool default_val = false) const;
        void setString(const std::string& key, const std::string& value);
        void setInt(const std::string& key, int value);
        void setBool(const std::string& key, bool value);
        void save(const std::string& base_dir = "");

        // 直接获取 JSON 对象（用于复杂结构）
        nlohmann::json getJson(const std::string& key) const;
        std::string getDefaultLanguage() const;

        // 读写嵌套 JSON 值（点号分隔，如 "dlls.ocr"）
        std::string getNestedString(const std::string& key_path, const std::string& default_val = "") const;
        void setNestedString(const std::string& key_path, const std::string& value);
        void setNestedBool(const std::string& key_path, bool value);
        nlohmann::json getNestedJson(const std::string& key_path) const;

    private:
        ConfigManager() = default;
        ~ConfigManager() = default;
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        nlohmann::json config_;
        bool loaded_ = false;
    };

} // namespace docmind