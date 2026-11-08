#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

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

using Microsoft::WRL::ComPtr;

struct Rhi
{
    ComPtr<IDXGIFactory6> m_factory;

    bool m_debugLayerEnabled{false};
    bool m_gpuValidationEnabled{false};
};

extern Rhi g_rhi;

struct Rhi_Init_Options
{
    bool m_debugLayer{false};
    bool m_gpuValidation{false};
};

void rhiInit(const Rhi_Init_Options& options);
void rhiShutdown();
