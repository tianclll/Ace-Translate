#include "docmind/utils/FormatFixer.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
std::string FormatFixer::replaceAll(
        const std::string& s,
        const std::string& from,
        const std::string& to)
{
    std::string result = s;

    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos)
    {
        result.replace(pos, from.length(), to);
    }

    return result;
}

std::string FormatFixer::onlineFormulaFix(const std::string& text)
{
    std::string s = text;

    s = replaceAll(s, "<fcel>", "");
    s = replaceAll(s, "</fcel>", "");
    s = replaceAll(s, "<nl>", "");
    std::cout << "s: " << s << std::endl;
    // ========== 新增：确保字符串两端有 \( 和 \) ==========
    if (s.compare(0, 2, "\\(") != 0 && s.compare(0, 2, "\\[") != 0) {
        s.insert(0, "\\(");
    }
    if (s.size() < 2 || s.compare(s.size() - 3, 3, "\\) ") != 0
        && s.compare(s.size() - 2, 2, "\\)") != 0 && s.compare(s.size() - 3, 3, "\\] ") != 0
                                                     && s.compare(s.size() - 2, 2, "\\]") != 0){
        s.append("\\) ");
    }
    // ===================================================

    return s;
}



static std::vector<std::string> split(const std::string& s,
                                      const std::string& delim)
{
    std::vector<std::string> result;

    size_t start = 0;
    size_t pos;

    while ((pos = s.find(delim, start)) != std::string::npos)
    {
        result.emplace_back(s.substr(start, pos - start));
        start = pos + delim.length();
    }

    result.emplace_back(s.substr(start));

    return result;
}

std::string FormatFixer::toMarkdownTable(const std::string& input)
{
    std::ostringstream out;

    // 行
    auto rows = split(input, "<nl>");

    bool firstRow = true;

    for (auto& row : rows)
    {
        if (row.empty())
            continue;

        // 去掉开头的 <fcel>
        if (row.rfind("<fcel>", 0) == 0)
            row.erase(0, 6);

        auto cols = split(row, "<fcel>");

        if (cols.empty())
            continue;

        out << "|";

        for (auto& col : cols)
            out << " " << col << " |";

        out << "\n";

        if (firstRow)
        {
            out << "|";

            for (size_t i = 0; i < cols.size(); ++i)
                out << " --- |";

            out << "\n";

            firstRow = false;
        }
    }

    return out.str();
}