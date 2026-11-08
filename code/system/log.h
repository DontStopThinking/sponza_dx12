#pragma once

#include <filesystem>
#include <format>
#include <string>

enum class Log_Level
{
    Info,
    Warning,
    Error,
};

struct Log_Line
{
    Log_Level m_Level{Log_Level::Info};
    std::string m_text{};
};

void logMessage(Log_Level level, const std::string& message);

template <typename... Args>
void logInfo(std::format_string<Args...> fmt, Args&&... args)
{
    logMessage(Log_Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logWarning(std::format_string<Args...> fmt, Args&&... args)
{
    logMessage(Log_Level::Warning, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logError(std::format_string<Args...> fmt, Args&&... args)
{
    logMessage(Log_Level::Error, std::format(fmt, std::forward<Args>(args)...));
}
