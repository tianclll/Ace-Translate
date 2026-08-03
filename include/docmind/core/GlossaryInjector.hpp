#pragma once

#include "docmind/engines/TranslatorEngine.hpp"
#include "docmind/core/GlossaryMatcher.hpp"
#include <string>
#include <vector>
#include <utility>
#include <mutex>

namespace docmind {

/**
 * @brief 术语注入器（线程安全）
 *
 * 设计：
 * - refreshFromDB() 在主线程调用，从 SQLite 读取所有术语到内存
 * - prepareGlossary() 可在任意线程调用，只做纯字符串匹配 + TranslatorEngine 设置
 * - 不涉及任何 SQLite/Qt SQL 操作，完全线程安全
 */
class GlossaryInjector {
public:
    /// 主线程调用：从数据库刷新术语缓存
    static void refreshFromDB();

    /// 翻译前准备：匹配术语并注入到 TranslatorEngine（可在工作线程调用）
    static int prepareGlossary(TranslatorEngine* translator,
                               const std::string& targetLang,
                               const std::string& sourceLang,
                               const std::string& text);

    /// 翻译完成后清除术语注入
    static void clearGlossary(TranslatorEngine* translator);

private:
    // 每条术语：{term, translation, targetLang, sourceLang}
    static std::vector<std::tuple<std::string, std::string, std::string, std::string>> allTerms_;
    static std::mutex mutex_;
};

} // namespace docmind
