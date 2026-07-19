#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ConversionResult {
    bool success;
    std::string markdown_path;      // 输出的 .md 文件路径
    int image_count;
    std::string error_message;      // 失败时的描述
    json metadata;                  // 其他元数据
};

class OfficeConverter {
public:
    // 构造函数：指定 exe 路径（若为空则在 PATH 中查找）
    explicit OfficeConverter(const std::string& exe_path = "office2md.exe");

    // 转换文件，返回 ConversionResult
    ConversionResult convert(const std::string& input_file,
                             const std::string& output_file = "");

private:
    std::string exe_path_;

    // 执行命令并捕获输出
    std::string executeCommand(const std::string& cmd_line);
};