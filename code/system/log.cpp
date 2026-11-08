#include "system/log.h"

#include <print>

#include <windows.h>

static const char* logLevelName(Log_Level level)
{
    switch (level)
    {
        case Log_Level::Info: return "INFO";
        case Log_Level::Warning: return "WARNING";
        case Log_Level::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

void logMessage(Log_Level level, const std::string& message)
{
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    std::string line = std::format(
        "[{:02}:{:02}:{:02}.{:03}] [{}] {}\n",
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        logLevelName(level),
        message);
    OutputDebugStringA(line.c_str());
    std::print("{}", line);

    // TODO(sbalse): File logger
}
