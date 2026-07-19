#include "docmind/utils/MarkdownGenerator.hpp"
#include <string>

namespace docmind {

    std::string MarkdownGenerator::generate(const std::vector<std::string>& segments) {
        std::string result;
        for (const auto& seg : segments) {
            if (seg.empty()) continue;
            result += seg + "\n\n";
        }
        // 移除末尾多余换行（可选）
        if (result.size() >= 2 && result.substr(result.size() - 2) == "\n\n") {
            result.resize(result.size() - 2);
        }
        return result;
    }

} // namespace docmind