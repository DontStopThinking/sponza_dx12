#include <cassert>
#include <cstdint>
#include <print>
#include <string_view>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>

#include "system/d3dutils.h"
#include "system/log.h"

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

    constexpr int WINDOW_WIDTH = 1600;
    constexpr int WINDOW_HEIGHT = 900;

    RECT rect{ 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
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

    // TODO(sbalse): Create D3D12 debug layer in debug builds

    ComPtr<IDXGIFactory6> factory;
    const UINT factoryFlags = 0; // TODO(sbalse): Use `DXGI_CREATE_FACTORY_DEBUG` flag in debug builds
    checkHr(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    checkHr(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));

    DXGI_ADAPTER_DESC1 adapterDesc = {};
    checkHr(adapter->GetDesc1(&adapterDesc));
    logInfo("GPU: {} ({} MB video memory)", toUtf8(adapterDesc.Description), adapterDesc.DedicatedVideoMemory / (1024 * 1024));

    // NOTE(sbalse): The Device is a virtual representation of the GPU
    ComPtr<ID3D12Device> device;
    checkHr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    setName(device.Get(), "Device");

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
    swapChainDesc.Width = WINDOW_WIDTH;
    swapChainDesc.Height = WINDOW_HEIGHT;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
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

    // NOTE(sbalse): Descriptor Heap to store Render Target Views.
    // A descriptor is a block of data that tells the GPU how to interpret resource memory. A descriptor heap
    // is an array of descriptors.
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    // NOTE(sbalse): We have the same no. of RTVs as the number of back buffers in our swapchain.
    rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    checkHr(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

    ComPtr<ID3D12Resource> renderTargets[2];

    const UINT rtvHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            checkHr(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));

            device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += rtvHandleIncrementSize;
        }
    }

    // NOTE(sbalse): Fence is used to synchronize the CPU with the GPU, so they don't touch the same memory at
    // the same time.
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue = 0;
    checkHr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // NOTE(sbalse): Win32 Events are used as the link between CPU fence and GPU fence
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    bool running = true;

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
    }

    DestroyWindow(windowHandle);

    return 0;
}
