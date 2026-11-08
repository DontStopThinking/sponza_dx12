#include "rhi/rhi.h"

#include <format>

#include "rhi/rhiutils.h"

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

static void createDevice()
{
    for (UINT i = 0;; i++)
    {
        ComPtr<IDXGIAdapter4> adapter;
        const HRESULT findAdapter = g_rhi.m_factory->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter));
        if (findAdapter == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC3 desc = {};
        adapter->GetDesc3(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
        {
            // Skip the software adapter
            continue;
        }

        const HRESULT deviceCreated = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&g_rhi.m_device));

        if (SUCCEEDED(deviceCreated))
        {
            g_rhi.m_adapter = adapter;
            g_rhi.m_adapterName = toUtf8(desc.Description);
            logInfo(
                "Adapter: {}: {} (vendor 0x{:04X}, device 0x{:04X}, {} MB dedicated VRAM, {} MB shared)",
                i,
                g_rhi.m_adapterName,
                desc.VendorId,
                desc.DeviceId,
                desc.DedicatedVideoMemory / (1024 * 1024),
                desc.SharedSystemMemory / (1024 * 1024));
            break;
        }

        logWarning(
            "Adapter {} ({}) does not support D3D12 feature level 12_0, skipping",
            i,
            toUtf8(desc.Description));
    }

    FATAL_CHECK(g_rhi.m_device, "No hardware adapter supports Direct3D 12 at feature level 12_0");
    g_rhi.m_device->SetName(TEXT("Device"));

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_HIGHEST_SHADER_MODEL};
    bool checkSMSupport = SUCCEEDED(g_rhi.m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));
    if (!checkSMSupport)
    {
        shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_6;
        checkSMSupport = SUCCEEDED(g_rhi.m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));
    }
    bool hasSM66 = checkSMSupport && shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6;
    FATAL_CHECK(hasSM66, "Shader Model 6.6 is required.");

    // Check for some more feature support
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    VERIFY_HR(g_rhi.m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));
    FATAL_CHECK(options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3, "Resource binding tier 3 is required");
    FATAL_CHECK(options.ResourceHeapTier >= D3D12_RESOURCE_BINDING_TIER_2, "Resource heap tier 2 is required");

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    g_rhi.m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    g_rhi.m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
    logInfo(
        "RHI: Features supported by GPU driver: shader model 6.{}, resource binding tier {}, resource heap tier {}, raytracing tier {}.{}, mesh shader tier {}",
        static_cast<uint32_t>(shaderModel.HighestShaderModel) & 0xF,
        static_cast<uint32_t>(options.ResourceBindingTier),
        static_cast<uint32_t>(options.ResourceHeapTier),
        static_cast<uint32_t>(options5.RaytracingTier) / 10,
        static_cast<uint32_t>(options5.RaytracingTier) % 10,
        static_cast<uint32_t>(options7.MeshShaderTier) / 10);
}

static void CALLBACK infoQueueDebugMessageCallback(
    D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
    D3D12_MESSAGE_ID,
    LPCSTR description,
    void*)
{
    if (severity == D3D12_MESSAGE_SEVERITY_CORRUPTION || severity == D3D12_MESSAGE_SEVERITY_ERROR)
    {
        g_rhi.m_infoQueueErrorCount.fetch_add(1, std::memory_order_relaxed);
        logError("RHI: [Info queue] {}", description);

    }
    else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
    {
        g_rhi.m_infoQueueWarningCount.fetch_add(1, std::memory_order_relaxed);
        logWarning("RHI: [Info queue] {}", description);
    }
}

static void configureInfoQueue()
{
    // NOTE(sbalse): The info queue gives us more debug messages. Probably useful I think.
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(g_rhi.m_device.As(&infoQueue)) && IsDebuggerPresent())
    {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    }

    ComPtr<ID3D12InfoQueue1> infoQueue1;
    if (SUCCEEDED(g_rhi.m_device.As(&infoQueue1)))
    {
        infoQueue1->RegisterMessageCallback(
            infoQueueDebugMessageCallback,
            D3D12_MESSAGE_CALLBACK_FLAG_NONE,
            nullptr,
            &g_rhi.m_messageCallbackCookie);
    }
    else
    {
        logWarning("RHI: ID3d12InfoQueue1 unavailable");
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

    createDevice();
    if (g_rhi.m_debugLayerEnabled)
    {
        configureInfoQueue();
    }
}

void rhiShutdown()
{
    if (g_rhi.m_debugLayerEnabled)
    {
        ComPtr<ID3D12DebugDevice> debugDevice;
        if (SUCCEEDED(g_rhi.m_device.As(&debugDevice)))
        {
            debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        }
    }

    g_rhi.m_factory.Reset();
    g_rhi.m_adapter.Reset();
    g_rhi.m_device.Reset();
}
