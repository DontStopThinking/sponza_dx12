#pragma once

#include <cstdint>

#include <windows.h>

struct App
{
    HWND m_windowHandle{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_minimized{0};
};

bool appInit(App& app, HWND windowHandle);
void appShutdown(App& app);
void appFrame(App& app);
bool appHandleMessage(App& app, HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
