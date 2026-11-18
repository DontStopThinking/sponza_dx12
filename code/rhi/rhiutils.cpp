#include "rhi/rhiutils.h"

void fatalError(const char* expression, const char* file, int line, const char* message)
{
    std::string text = std::format("RHI: {}\n\n{}\n{}({})", message, expression, file, line);
    logMessage(Log_Level::Error, "FATAL: " + text);
    if (IsDebuggerPresent())
    {
        __debugbreak();
    }
    ExitProcess(EXIT_FAILURE);
}

std::string toUtf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }

    const int len = static_cast<int>(text.size());
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), len, nullptr, 0, nullptr, nullptr);

    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), len, result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring toWide(std::string_view text)
{
    if (text.empty())
    {
        return {};
    }

    const int len = static_cast<int>(text.size());
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), len, nullptr, 0);

    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), len, result.data(), count);
    return result;
}
