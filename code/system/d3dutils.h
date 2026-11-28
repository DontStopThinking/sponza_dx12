#pragma once

#include <format>
#include <string>
#include <source_location>
#include <string_view>

#include <d3d12.h>
#include <dxgi.h>
#include <WRL/client.h>

#include "system/log.h"

#if defined(SPONZA_DEBUG)
#define SPONZA_DEBUG_ONLY(...) __VA_ARGS__
#else
#define SPONZA_DEBUG_ONLY(...)
#endif // SPONZA_DEBUG

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

inline void logFatalError(std::string_view msg, std::source_location loc = std::source_location::current())
{
    logFatal("{}\n\nat {}:{}\nin {}", msg, loc.file_name(), loc.line(), loc.function_name());
}

// TODO(sbalse): Should this be debug only?
#define SPONZA_BREAK_AND_ABORT() \
    do \
    { \
        if (IsDebuggerPresent()) \
        { \
            __debugbreak(); \
        } \
        std::abort(); \
    } while (0);

#define FATAL_ERROR(msg) \
    do \
    { \
        logFatalError((msg)); \
    } while (0)

#define CHECK_HR(expr) \
    do \
    { \
        const HRESULT hr = (expr); \
        if (FAILED(hr)) [[unlikely]] \
        { \
            logFatalError(std::format("{} failed: 0x{:08X}", #expr, static_cast<uint32_t>(hr))); \
            SPONZA_BREAK_AND_ABORT(); \
        } \
    } while (0)

std::wstring toWide(std::string_view text);
std::string toUtf8(std::wstring_view text);

inline void setName(ID3D12Object* object, std::string_view name)
{
    object->SetName(toWide(name).c_str());
}

inline void setName(IDXGIObject* object, std::string_view name)
{
    object->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
}
