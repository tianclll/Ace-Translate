#pragma once

#include <string>
#include <vector>

namespace docmind {

    class MarkdownGenerator {
    public:
        // 将多个段落合并为 Markdown，每个段落间空一行，返回内容字符串
        static std::string generate(const std::vector<std::string>& segments);
    };

} // namespace docmind