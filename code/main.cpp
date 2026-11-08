#include <cassert>
#include <cstdlib>
#include <print>

#include <windows.h>

#include "app.h"

static App g_app = {};

static LRESULT CALLBACK windowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    if (appHandleMessage(g_app, windowHandle, message, wParam, lParam, result))
    {
        // Nothing to do, the app has handled the Windows messages
    }
    else
    {
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

        case WM_QUIT:
        case WM_CLOSE:
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
    }

    return result;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
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

    int exitCode = EXIT_SUCCESS;
    if (appInit(g_app, windowHandle))
    {
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

            if (g_app.m_minimized)
            {
                Sleep(16); // Nothing to present, don't update
                continue;
            }
            appFrame(g_app);
        }
    }
    else
    {
        // TODO(sbalse): logging
        exitCode = EXIT_FAILURE;
    }

    appShutdown(g_app);
    DestroyWindow(windowHandle);

    return exitCode;
}
