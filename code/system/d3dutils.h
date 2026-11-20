#pragma once

#include <format>
#include <string>
#include <source_location>
#include <string_view>

#include <d3d12.h>
#include <WRL/client.h>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

[[noreturn]] void fatalError(
    std::string_view message, std::source_location location = std::source_location::current());

inline void checkHr(HRESULT result, std::source_location location = std::source_location::current())
{
    if (SUCCEEDED(result))
    {
        return;
    }
    fatalError(std::format("HRESULT failed: 0x{:08X}", static_cast<uint32_t>(result)), location);
}

std::wstring toWide(std::string_view text);

std::string toUtf8(std::wstring_view text);

inline void setName(ID3D12Object* object, std::string_view name)
{
    object->SetName(toWide(name).c_str());
}
