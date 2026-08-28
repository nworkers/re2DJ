#include "window_mode.h"

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_fullscreen = FALSE;

namespace
{

constexpr char kWindowTitle[] = "re2DJ";
constexpr DWORD kWindowedStyle =
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr DWORD kFullscreenStyle = WS_POPUP;
constexpr DWORD kExtendedStyle = WS_EX_APPWINDOW;

bool SetWindowAttribute(HWND window, int index, LONG_PTR value)
{
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(window, index, value);
    return previous != 0 || GetLastError() == ERROR_SUCCESS;
}

}  // namespace

bool ApplyRe2djWindowMode(HWND window, DWORD client_width, DWORD client_height)
{
    if (window == nullptr || !IsWindow(window) || client_width == 0 || client_height == 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor == nullptr || GetMonitorInfoA(monitor, &monitor_info) == FALSE)
    {
        return false;
    }

    const bool fullscreen = g_re2dj_fullscreen != FALSE;
    const DWORD style = fullscreen ? kFullscreenStyle : kWindowedStyle;
    RECT bounds = fullscreen ? monitor_info.rcMonitor : RECT{0, 0,
                                                             static_cast<LONG>(client_width),
                                                             static_cast<LONG>(client_height)};
    if (!fullscreen && AdjustWindowRectEx(&bounds, style, FALSE, kExtendedStyle) == FALSE)
    {
        return false;
    }

    const RECT placement = fullscreen ? monitor_info.rcMonitor : monitor_info.rcWork;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int x = fullscreen ? placement.left
                             : placement.left + ((placement.right - placement.left) - width) / 2;
    const int y = fullscreen ? placement.top
                             : placement.top + ((placement.bottom - placement.top) - height) / 2;

    if (!SetWindowAttribute(window, GWL_STYLE, style) ||
        !SetWindowAttribute(window, GWL_EXSTYLE, kExtendedStyle) ||
        SetWindowTextA(window, kWindowTitle) == FALSE)
    {
        return false;
    }
    return SetWindowPos(window,
                        HWND_TOP,
                        x,
                        y,
                        width,
                        height,
                        SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
}
