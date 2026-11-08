#include "rhi/rhi.h"

/*extern*/ Rhi g_rhi = {};

static void enableDebugLayer(const Rhi_Init_Options& options)
{
    ComPtr<ID3D12Debug1> debug;
    if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        logWarning("RHI: D3D12 debug layer unavailable");
        return;
    }
    debug->EnableDebugLayer();
    g_rhi.m_debugLayerEnabled = true;
    logInfo("RHI: D3D12 debug layer enabled (GPU-based validation: {})", options.m_gpuValidation);

    if (options.m_gpuValidation)
    {
        debug->SetEnableGPUBasedValidation(TRUE);
        g_rhi.m_gpuValidationEnabled = true;
    }
}

void rhiInit(const Rhi_Init_Options& options)
{
    UINT factoryFlags = 0;
    if (options.m_debugLayer)
    {
        enableDebugLayer(options);
        if (g_rhi.m_debugLayerEnabled)
        {
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
    VERIFY_HR(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&g_rhi.m_factory)));
}

void rhiShutdown()
{
    g_rhi.m_factory.Reset();
}
