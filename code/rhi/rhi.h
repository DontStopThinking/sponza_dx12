#pragma once

#include <atomic>
#include <string>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using namespace Microsoft;
using namespace Microsoft::WRL;

struct Rhi
{
    ComPtr<IDXGIFactory6> m_factory;
    ComPtr<IDXGIAdapter4> m_adapter;
    ComPtr<ID3D12Device5> m_device;
    std::string m_adapterName;

    bool m_debugLayerEnabled{false};
    bool m_gpuValidationEnabled{false};

    std::atomic<uint32_t> m_infoQueueErrorCount{0};
    std::atomic<uint32_t> m_infoQueueWarningCount{0};
    DWORD m_messageCallbackCookie{0};
};

extern Rhi g_rhi;

struct Rhi_Init_Options
{
    bool m_debugLayer{false};
    bool m_gpuValidation{false};
};

void rhiInit(const Rhi_Init_Options& options);
void rhiShutdown();
