#pragma once

#include <string>
#include <string_view>

#include <windows.h>

#include "system/log.h"

#define VERIFY_HR(expr) \
    do \
    { \
        const HRESULT hr_ = (expr); \
            if (FAILED(hr_)) \
            { \
                logError("HRESULT: 0x{:08X}", static_cast<uint32_t>(hr_)); \
                    if (IsDebuggerPresent()) \
                    { \
                        __debugbreak(); \
                    } \
            } \
    } while (false) \

[[noreturn]] void fatalError(const char* expression, const char* file, int line, const char* message);

#define FATAL_CHECK(cond, message) \
    do \
    { \
        if (!(cond)) \
        { \
            fatalError(#cond, __FILE__, __LINE__, message); \
        } \
    } while (false) \

std::string toUtf8(std::wstring_view text);
