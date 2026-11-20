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

    RECT rect{ 0, 0, 1600, 900 };
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
    checkHr(commandList->Close());

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
