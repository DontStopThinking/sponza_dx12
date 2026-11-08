#include "app.h"

bool appInit(App& app, HWND windowHandle)
{
    app.m_windowHandle = windowHandle;
    RECT rect = {};
    GetClientRect(windowHandle, &rect);
    app.m_width = static_cast<uint32_t>(rect.right - rect.left);
    app.m_height = static_cast<uint32_t>(rect.bottom - rect.top);

    return true;
}

void appShutdown(App& app)
{
    // Shutdown everything here
}

void appFrame(App& app)
{
    // Render the frame here
}

bool appHandleMessage(App& app, HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    switch (message)
    {
        case WM_SIZE:
        {
            const uint32_t width = LOWORD(lParam);
            const uint32_t height = HIWORD(lParam);
            const bool minimized = (wParam == SIZE_MINIMIZED || width == 0 || height == 0);
            app.m_minimized = minimized;
            if (!app.m_minimized && (width != app.m_width || height != app.m_height))
            {
                app.m_width = width;
                app.m_height = height;
            }

            result = 0;

            return false;
        }

        default:
        {
            result = 0;
            return false;
        }
    }
}
