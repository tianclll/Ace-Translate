#include "docmind/utils/OfficeConverter.h"
#include <windows.h>
#include <sstream>
#include <iostream>
#include <array>

OfficeConverter::OfficeConverter(const std::string& exe_path)
        : exe_path_(exe_path) {}

std::string OfficeConverter::executeCommand(const std::string& cmd_line) {
    std::string result;
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return "";

    // 保证管道句柄可继承
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    // 构造命令行（宽字符）
    std::wstring wcmd = std::wstring(cmd_line.begin(), cmd_line.end());
    std::vector<wchar_t> cmd_buf(wcmd.begin(), wcmd.end());
    cmd_buf.push_back(L'\0');

    BOOL bRet = CreateProcessW(
            NULL,                     // 可执行文件路径（NULL 表示从命令行解析）
            cmd_buf.data(),           // 命令行
            NULL, NULL, TRUE,         // 继承句柄
            CREATE_NO_WINDOW,         // 不显示窗口（静默运行）
            NULL, NULL, &si, &pi
    );

    CloseHandle(hWritePipe);  // 关闭写端，让子进程结束

    if (!bRet) {
        CloseHandle(hReadPipe);
        return "";
    }

    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 读取输出
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

ConversionResult OfficeConverter::convert(const std::string& input_file,
                                          const std::string& output_file) {
    ConversionResult res;
    res.success = false;

    // 构建命令行
    std::string cmd = "\"" + exe_path_ + "\" --input \"" + input_file + "\"";
    if (!output_file.empty()) {
        cmd += " --output \"" + output_file + "\"";
    }

    std::string output = executeCommand(cmd);
    if (output.empty()) {
        res.error_message = "Failed to run converter.";
        return res;
    }

    try {
        auto j = json::parse(output);
        if (j.contains("status") && j["status"] == "success") {
            res.success = true;
            res.markdown_path = j.value("markdown", "");
            res.image_count = j.value("image_count", 0);
            res.metadata = j.value("metadata", json::object());
        } else {
            res.success = false;
            res.error_message = j.value("message", "Unknown error");
        }
    } catch (const std::exception& e) {
        res.success = false;
        res.error_message = std::string("JSON parse error: ") + e.what();
    }

    return res;
}