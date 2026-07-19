#pragma once

#include <string>


class FormatFixer {
public:
    static std::string onlineFormulaFix(const std::string &text);

    static std::string toMarkdownTable(const std::string &input);

private:
    static std::string replaceAll(
            const std::string &s,
            const std::string &from,
            const std::string &to);
};
