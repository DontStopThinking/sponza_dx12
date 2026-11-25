#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <print>
#include <string_view>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include "system/d3dutils.h"
#include "system/log.h"

struct WindowContext
{
    UINT m_width{0};
    UINT m_height{0};
    bool resizePending{false};
    bool m_minimized{false};
};
static WindowContext g_windowContext = {};

static void onDebugLayerMessage(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID,
                                LPCSTR description, void*)
{
    if (severity == D3D12_MESSAGE_SEVERITY_CORRUPTION || severity == D3D12_MESSAGE_SEVERITY_ERROR)
    {
        logError("[D3D12] {}", description);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
    {
        logWarning("[D3D12] {}", description);
    }
    else
    {
        logInfo("[D3D12] {}", description);
    }
}

static void createRenderTargetViews(ID3D12Device* device, IDXGISwapChain3* swapChain,
                                    ID3D12DescriptorHeap* rtvHeap, UINT rtvHandleIncrementSize,
                                    ComPtr<ID3D12Resource>* renderTargets, UINT backBufferCount)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < backBufferCount; i++)
    {
        checkHr(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        setName(renderTargets[i].Get(), std::format("Back Buffer {}", i));
        rtvHandle.ptr += rtvHandleIncrementSize;
    }
}

static void setBlendState(D3D12_BLEND_DESC& blendDesc)
{
    blendDesc = {};

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {};
    defaultRenderTargetBlendDesc.BlendEnable = FALSE;
    defaultRenderTargetBlendDesc.LogicOpEnable = FALSE;
    defaultRenderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
    defaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
    defaultRenderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    defaultRenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    defaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    defaultRenderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    defaultRenderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    defaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
    }
}

static void setRasterizerState(D3D12_RASTERIZER_DESC& rasterizerDesc)
{
    rasterizerDesc = {};

    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}

static void setDepthStencilState(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc)
{
    depthStencilDesc = {};

    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

    depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
}

static void waitForGpu(ID3D12CommandQueue* commandQueue, ID3D12Fence* fence, UINT64& fenceValue, HANDLE fenceEvent)
{
    const UINT64 valueToWaitFor = ++fenceValue;
    checkHr(commandQueue->Signal(fence, valueToWaitFor));

    if (fence->GetCompletedValue() < valueToWaitFor)
    {
        checkHr(fence->SetEventOnCompletion(valueToWaitFor, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

static void createDebugConsole()
{
    // Reuse the console if launched from cmd, otherwise create a new one
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        AllocConsole();
    }

    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    // freopen_s(&fp, "CONIN$", "r", stderr); // NOTE(sbalse): Probably not ever using the debug console for input

    SetConsoleTitle(TEXT("Debug Console"));
}

static LRESULT CALLBACK windowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    switch (message)
    {
        case WM_KEYDOWN:
        {
            // ESCAPE to exit
            if (wParam == VK_ESCAPE)
            {
                PostQuitMessage(0);
                result = 0;
            }

        } break;

        case WM_SIZE:
        {
            // NOTE(sbalse): We need to resize the D3D backbuffers, so we don't immediately resize here. We
            // set some flags and do the resizing before rendering.
            g_windowContext.m_minimized = (wParam == SIZE_MINIMIZED);
            if (!g_windowContext.m_minimized)
            {
                const UINT width = LOWORD(lParam);
                const UINT height = HIWORD(lParam);
                if (width != g_windowContext.m_width || height != g_windowContext.m_height)
                {
                    g_windowContext.m_width = width;
                    g_windowContext.m_height = height;
                    g_windowContext.resizePending = true;
                }
            }
        } break;

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            result = 0;
        } break;

        default:
        {
            result = DefWindowProc(windowHandle, message, wParam, lParam);
        } break;
    }

    return result;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    createDebugConsole();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEX windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = TEXT("SponzaD3D12Window");
    RegisterClassEx(&windowClass);

    constexpr int INITIAL_WINDOW_WIDTH = 1600;
    constexpr int INITIAL_WINDOW_HEIGHT = 900;

    RECT rect{ 0, 0, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND windowHandle = CreateWindowEx(
        0,
        windowClass.lpszClassName,
        TEXT("Sponza DX12"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    assert(windowHandle != nullptr && "CreateWindow failed");

    ShowWindow(windowHandle, showCommand);

    RECT clientRect = {};
    GetClientRect(windowHandle, &clientRect);
    g_windowContext.m_width = static_cast<UINT>(clientRect.right - clientRect.left);
    g_windowContext.m_height = static_cast<UINT>(clientRect.bottom - clientRect.top);
    g_windowContext.resizePending = false;

    ComPtr<IDXGIFactory6> factory;
    UINT factoryFlags = 0;
    SPONZA_DEBUG_ONLY(factoryFlags |= DXGI_CREATE_FACTORY_DEBUG);
    checkHr(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    checkHr(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                IID_PPV_ARGS(&adapter)));

    DXGI_ADAPTER_DESC1 adapterDesc = {};
    checkHr(adapter->GetDesc1(&adapterDesc));
    logInfo("GPU: {} ({} MB video memory)", toUtf8(adapterDesc.Description),
            adapterDesc.DedicatedVideoMemory / (1024 * 1024));

#if defined(SPONZA_DEBUG)
    ComPtr<ID3D12Debug1> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
        // debug->SetEnableGPUBasedValidation(TRUE); // NOTE(sbalse): Commenting this out since it increases startup time a bit
        logInfo("D3D12 debug layer has been enabled");
    }
    else
    {
        logWarning("D3D12 debug layer is not available");
    }
#endif // SPONZA_DEBUG

    // NOTE(sbalse): The Device is a virtual representation of the GPU
    ComPtr<ID3D12Device> device;
    checkHr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    setName(device.Get(), "Device");

    SPONZA_DEBUG_ONLY(DWORD debugCallbackCookie = 0);

#if defined(SPONZA_DEBUG)
    // NOTE(sbalse): Route debug layer messages to our logs and break in the debugger on errors
    ComPtr<ID3D12InfoQueue1> infoQueue;
    if (SUCCEEDED(device.As(&infoQueue)))
    {
        infoQueue->RegisterMessageCallback(
            onDebugLayerMessage,
            D3D12_MESSAGE_CALLBACK_FLAG_NONE,
            nullptr,
            &debugCallbackCookie);

        if (IsDebuggerPresent())
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        }
    }
    else
    {
        logInfo("ID3D12InfoQueue1 is not available; debug layer messages will only appear in the VS output window");
    }
#endif // SPONZA_DEBUG

    // NOTE(sbalse): The command queue decides which order the command lists should execute in
    ComPtr<ID3D12CommandQueue> commandQueue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    checkHr(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
    setName(commandQueue.Get(), "Direct Command Queue");

    // NOTE(sbalse): Command allocator is used to allocate memory on the GPU for commands
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    checkHr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    checkHr(commandAllocator->Reset());
    setName(commandAllocator.Get(), "Command Allocator");

    // NOTE(sbalse): Command list is used to store a list of commands that we want to execute on the GPU
    ComPtr<ID3D12GraphicsCommandList> commandList;
    checkHr(device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)));
    setName(commandList.Get(), "Direct Command List");
    checkHr(commandList->Close()); // Start out closed

    constexpr int BACK_BUFFER_COUNT = 2;

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = INITIAL_WINDOW_WIDTH;
    swapChainDesc.Height = INITIAL_WINDOW_HEIGHT;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
    checkHr(factory->CreateSwapChainForHwnd(
        commandQueue.Get(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain1));

    checkHr(factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));

    // Upgrade to IDXGISwapChain3
    ComPtr<IDXGISwapChain3> swapChain;
    checkHr(swapChain1.As(&swapChain));
    setName(swapChain.Get(), "SwapChain");

    // NOTE(sbalse): Descriptor Heap to store Render Target Views.
    // A descriptor is a block of data that tells the GPU how to interpret resource memory. A descriptor heap
    // is an array of descriptors.
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    // NOTE(sbalse): We have the same no. of RTVs as the number of back buffers in our swapchain.
    rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    checkHr(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));
    setName(rtvHeap.Get(), "Render Target View Heap");

    ComPtr<ID3D12Resource> renderTargets[BACK_BUFFER_COUNT];

    const UINT rtvHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    createRenderTargetViews(device.Get(), swapChain.Get(), rtvHeap.Get(), rtvHandleIncrementSize,
                            renderTargets, BACK_BUFFER_COUNT);

    // NOTE(sbalse): Fence is used to synchronize the CPU with the GPU, so they don't touch the same memory at
    // the same time.
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue = 0;
    checkHr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    setName(fence.Get(), "Fence");

    // NOTE(sbalse): Win32 Events are used as the link between CPU fence and GPU fence
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // NOTE(sbalse): A root signature determines the type of data shaders should expect. A root parameter is
    // one entry in the root signature.
    // For our rotating triangle, we need only a single root parameter constant for our angle.
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.Num32BitValues = 1;
    rootParameters[0].Constants.ShaderRegister = 0;
    rootParameters[0].Constants.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = ARRAY_COUNT(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    {
        ComPtr<ID3DBlob> errorBlob;
        const HRESULT hr = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1_0,
            &signatureBlob,
            &errorBlob);
        if (FAILED(hr))
        {
            const std::string message = errorBlob
                ? std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize())
                : "";
            fatalError(std::format("Root signature serialization failed: {}", message));
        }
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    checkHr(device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)));
    setName(rootSignature.Get(), "Root Signature");

    // Compile shaders
    logInfo("Compiling shaders...");
    ComPtr<ID3DBlob> vertexShader;
    {
        ComPtr<ID3DBlob> errors;
        const HRESULT hr = D3DCompileFromFile(
            L"shaders/shader.hlsl",
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            0,
            0,
            &vertexShader,
            &errors);
        if (FAILED(hr))
        {
            const std::string message = errors
                ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                : "";
            fatalError(
                std::format(
                    "Vertex shader compilation failed (HRESULT 0x{:08X}): {}",
                    static_cast<uint32_t>(hr),
                    message));
        }
    }

    ComPtr<ID3DBlob> pixelShader;
    {
        ComPtr<ID3DBlob> errors;
        const HRESULT hr = D3DCompileFromFile(
            L"shaders/shader.hlsl",
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            0,
            0,
            &pixelShader,
            &errors);
        if (FAILED(hr))
        {
            const std::string message = errors
                ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                : "";
            fatalError(
                std::format(
                    "Pixel shader compilation failed (HRESULT 0x{:08X}): {}",
                    static_cast<uint32_t>(hr),
                    message));
        }
    }

    logInfo("Shaders compiled successfully!");

    // Pipline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = rootSignature.Get();
    pipelineStateDesc.VS.pShaderBytecode = vertexShader->GetBufferPointer();
    pipelineStateDesc.VS.BytecodeLength = vertexShader->GetBufferSize();
    pipelineStateDesc.PS.pShaderBytecode = pixelShader->GetBufferPointer();
    pipelineStateDesc.PS.BytecodeLength = pixelShader->GetBufferSize();
    pipelineStateDesc.SampleMask = UINT_MAX;
    pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineStateDesc.InputLayout.NumElements = 0;
    pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleDesc.Quality = 0;
    setBlendState(pipelineStateDesc.BlendState);
    setRasterizerState(pipelineStateDesc.RasterizerState);
    setDepthStencilState(pipelineStateDesc.DepthStencilState);

    ComPtr<ID3D12PipelineState> pipelineState;
    checkHr(device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState)));
    setName(pipelineState.Get(), "Pipeline State");

    bool running = true;

    constexpr float ROTATION_SPEED = 1.0;
    float triangleAngle = 0;

    auto previousTime = std::chrono::steady_clock::now();
    while (running)
    {
        MSG message = {};
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = false;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        if (!running)
        {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        const float frameSeconds = std::chrono::duration<float>(now - previousTime).count();
        const float deltaSeconds = std::min(frameSeconds, 0.1f);
        previousTime = now;

        triangleAngle += ROTATION_SPEED * deltaSeconds;
        triangleAngle = std::fmod(triangleAngle, 2.0f * std::numbers::pi_v<float>);

        if (g_windowContext.m_minimized)
        {
            continue;
        }

        // NOTE(sbalse): Handle window resizing
        if (g_windowContext.resizePending)
        {
            g_windowContext.resizePending = false;

            // NOTE(sbalse): We must wait for the GPU to be done before resizing the backbuffers.
            waitForGpu(commandQueue.Get(), fence.Get(), fenceValue, fenceEvent);

            // Release back buffers before resizing
            for (UINT i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                renderTargets[i].Reset();
            }

            checkHr(swapChain->ResizeBuffers(BACK_BUFFER_COUNT, g_windowContext.m_width,
                                             g_windowContext.m_height, DXGI_FORMAT_UNKNOWN,
                                             swapChainDesc.Flags));

            // Recreate the RTVs
            createRenderTargetViews(device.Get(), swapChain.Get(), rtvHeap.Get(), rtvHandleIncrementSize,
                                    renderTargets, BACK_BUFFER_COUNT);
        }

        // Record commands to draw a triangle
        checkHr(commandAllocator->Reset());
        checkHr(commandList->Reset(commandAllocator.Get(), nullptr));

        UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = renderTargets[backBufferIndex].Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += backBufferIndex * rtvHandleIncrementSize;

        // Clear the render target
        constexpr float CLEAR_COLOR[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        // Set viewport and scissor
        const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(g_windowContext.m_width),
                                          static_cast<float>(g_windowContext.m_height), 0.0f, 1.0f };
        const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(g_windowContext.m_width),
                                         static_cast<LONG>(g_windowContext.m_width) };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootSignature(rootSignature.Get());
        commandList->SetPipelineState(pipelineState.Get());

        // Draw the triangle
        commandList->SetGraphicsRoot32BitConstant(0, std::bit_cast<UINT>(triangleAngle), 0);
        commandList->DrawInstanced(3, 1, 0, 0);

        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = renderTargets[backBufferIndex].Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);
        }

        checkHr(commandList->Close());

        ID3D12CommandList* commandLists[] = { commandList.Get() };
        // NOTE(sbalse): This line actually sends the current frame's commands to the GPU queue
        commandQueue->ExecuteCommandLists(1, commandLists);

        checkHr(swapChain->Present(1, 0));

        // Wait on the CPU for the GPU frame to finish
        const UINT64 valueToWaitFor = ++fenceValue;
        // NOTE(sbalse): Add one more command to the end of the queue to write `currentFenceValue` into the fence
        checkHr(commandQueue->Signal(fence.Get(), valueToWaitFor));

        // NOTE(sbalse): This immediately checks if the GPU's fence value is less than the CPU's. If it is, then
        // that means the GPU is behind the CPU, and so we put the CPU to sleep.
        waitForGpu(commandQueue.Get(), fence.Get(), fenceValue, fenceEvent);
    }

    CloseHandle(fenceEvent);
    DestroyWindow(windowHandle);

#if defined(SPONZA_DEBUG)
    ComPtr<ID3D12DebugDevice> debugDevice;
    if (SUCCEEDED(device.As(&debugDevice)))
    {
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }

    if (debugCallbackCookie != 0)
    {
        infoQueue->UnregisterMessageCallback(debugCallbackCookie);
    }
#endif // SPONZA_DEBUG

    return 0;
}
