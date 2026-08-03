#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <utility>

namespace docmind {

/**
 * @brief 术语匹配器：从文本中匹配术语库中的术语
 *
 * 术语库已按原文长度降序排列（由 SQL ORDER BY length(term) DESC）。
 * 对每条术语做大小写不敏感的子串匹配，返回命中列表（最多 maxMatches 条）。
 * 将文本中的换行符替换为空格后再搜索，支持跨行匹配。
 */
inline std::vector<std::pair<std::string, std::string>>
matchGlossaryTerms(const std::string& text,
                   const std::vector<std::pair<std::string, std::string>>& terms,
                   int maxMatches = 20) {
    std::vector<std::pair<std::string, std::string>> matched;

    if (text.empty() || terms.empty() || maxMatches <= 0)
        return matched;

    // 将文本换行替换为空格（支持跨行术语匹配）
    std::string normalized = text;
    for (char& c : normalized) {
        if (c == '\n' || c == '\r')
            c = ' ';
    }

    // 转小写用于大小写不敏感搜索
    std::string lower_text;
    lower_text.reserve(normalized.size());
    std::transform(normalized.begin(), normalized.end(),
                   std::back_inserter(lower_text), ::tolower);

    for (const auto& [term, translation] : terms) {
        if (matched.size() >= static_cast<size_t>(maxMatches))
            break;

        // 跳过空术语
        if (term.empty()) continue;

        // 大小写不敏感搜索
        std::string lower_term;
        lower_term.reserve(term.size());
        std::transform(term.begin(), term.end(),
                       std::back_inserter(lower_term), ::tolower);

        if (lower_text.find(lower_term) != std::string::npos) {
            matched.emplace_back(term, translation);
        }
    }

    return matched;
}

} // namespace docmind
