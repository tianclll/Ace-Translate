#pragma once

#include <string>

namespace docmind {

    class TextTranslationModule {
    public:
        /**
         * @brief 构造函数
         * @param target_language 目标语言（如 "English"、"中文"），默认从配置读取
         */
        explicit TextTranslationModule(const std::string& target_language = "");

        /**
         * @brief 翻译文本
         * @param text       待翻译文本
         * @param max_tokens 最大token数
         * @return 翻译后的文本
         */
        std::string translate(const std::string& text, int max_tokens = 512);

    private:
        std::string target_language_;
    };

} // namespace docmind