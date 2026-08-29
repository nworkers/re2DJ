#include "window_mode.h"

#include <cstdio>
#include <cstring>

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_fullscreen = FALSE;
extern "C" char g_re2dj_graphics_trace_path[MAX_PATH];

namespace
{

constexpr char kWindowTitle[] = "re2DJ";
constexpr DWORD kWindowedStyle =
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr DWORD kFullscreenStyle = WS_POPUP;
constexpr DWORD kExtendedStyle = WS_EX_APPWINDOW;
constexpr char kOriginalWindowProcedureProperty[] = "re2dj.original-window-procedure";
constexpr DWORD kWindowLifetimePollMilliseconds = 50;
constexpr DWORD kWindowLifetimeSamplePolls = 20;
constexpr DWORD kWindowLifetimeMaximumSamples = 600;
PVOID volatile g_watched_window = nullptr;
volatile LONG g_window_lifetime_watcher_started = FALSE;

void TraceWindowLifetime(const char* event, HWND window, BOOL valid, BOOL visible)
{
    char line[320] = {};
    const LONG_PTR procedure = valid == FALSE ? 0 : GetWindowLongPtrA(window, GWLP_WNDPROC);
    const int length = std::snprintf(
        line,
        sizeof(line),
        "re2dj:hle:window-lifetime:event=%s:pid=%lu:thread=%lu:hwnd=%p:valid=%lu:visible=%lu:wndproc=0x%08lx\r\n",
        event,
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        reinterpret_cast<void*>(window),
        static_cast<unsigned long>(valid),
        static_cast<unsigned long>(visible),
        static_cast<unsigned long>(procedure));
    if (length <= 0)
    {
        return;
    }
    OutputDebugStringA(line);
    if (g_re2dj_graphics_trace_path[0] == '\0')
    {
        return;
    }
    const HANDLE file = CreateFileA(g_re2dj_graphics_trace_path,
                                    FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr,
                                    OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
    CloseHandle(file);
}

[[noreturn]] void TerminateCurrentProcessForHostClose()
{
    if (TerminateProcess(GetCurrentProcess(), 0) == FALSE)
    {
        ExitProcess(1);
    }
    while (true)
    {
        Sleep(INFINITE);
    }
}

DWORD WINAPI WatchWindowLifetime(void*)
{
    DWORD polls = 0;
    DWORD samples = 0;
    while (true)
    {
        const HWND window = reinterpret_cast<HWND>(InterlockedCompareExchangePointer(
            &g_watched_window, nullptr, nullptr));
        const BOOL valid = window == nullptr ? FALSE : IsWindow(window);
        const BOOL visible = valid == FALSE ? FALSE : IsWindowVisible(window);
        if (samples < kWindowLifetimeMaximumSamples &&
            polls % kWindowLifetimeSamplePolls == 0)
        {
            TraceWindowLifetime("sample", window, valid, visible);
            ++samples;
        }
        if (window != nullptr && (valid == FALSE || visible == FALSE))
        {
            TraceWindowLifetime("watcher-exit", window, valid, visible);
            TerminateCurrentProcessForHostClose();
        }
        ++polls;
        Sleep(kWindowLifetimePollMilliseconds);
    }
}

bool StartWindowLifetimeWatcher(HWND window)
{
    InterlockedExchangePointer(&g_watched_window, reinterpret_cast<PVOID>(window));
    TraceWindowLifetime("target", window, IsWindow(window), IsWindowVisible(window));
    if (InterlockedCompareExchange(&g_window_lifetime_watcher_started, TRUE, FALSE) != FALSE)
    {
        return true;
    }
    const HANDLE thread = CreateThread(nullptr, 0, WatchWindowLifetime, nullptr, 0, nullptr);
    if (thread == nullptr)
    {
        InterlockedExchangePointer(&g_watched_window, nullptr);
        InterlockedExchange(&g_window_lifetime_watcher_started, FALSE);
        return false;
    }
    CloseHandle(thread);
    return true;
}

LRESULT CALLBACK Re2djWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    const bool close_requested =
        message == WM_CLOSE ||
        (message == WM_SYSCOMMAND && (wparam & 0xfff0U) == SC_CLOSE);
    if (close_requested)
    {
        TraceWindowLifetime("close-message", window, IsWindow(window), IsWindowVisible(window));
    }
    const auto original = reinterpret_cast<WNDPROC>(reinterpret_cast<ULONG_PTR>(
        GetPropA(window, kOriginalWindowProcedureProperty)));
    const LRESULT result = original == nullptr
                               ? DefWindowProcA(window, message, wparam, lparam)
                               : CallWindowProcA(original, window, message, wparam, lparam);
    if (close_requested)
    {
        TerminateCurrentProcessForHostClose();
    }
    else if (message == WM_NCDESTROY)
    {
        RemovePropA(window, kOriginalWindowProcedureProperty);
    }
    return result;
}

bool InstallWindowProcedureAdapter(HWND window)
{
    if (GetPropA(window, kOriginalWindowProcedureProperty) != nullptr)
    {
        return true;
    }
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR original = SetWindowLongPtrA(
        window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Re2djWindowProcedure));
    if (original == 0 && GetLastError() != ERROR_SUCCESS)
    {
        return false;
    }
    if (SetPropA(window,
                 kOriginalWindowProcedureProperty,
                 reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(original))) == FALSE)
    {
        SetWindowLongPtrA(window, GWLP_WNDPROC, original);
        return false;
    }
    return true;
}

bool SetWindowAttribute(HWND window, int index, LONG_PTR value)
{
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(window, index, value);
    return previous != 0 || GetLastError() == ERROR_SUCCESS;
}

}  // namespace

extern "C" __declspec(dllexport) void WINAPI Re2djExitIfWindowClosed(HWND window)
{
    const BOOL valid = window == nullptr ? FALSE : IsWindow(window);
    const BOOL visible = valid == FALSE ? FALSE : IsWindowVisible(window);
    if (window != nullptr && (valid == FALSE || visible == FALSE))
    {
        TraceWindowLifetime("flip-exit", window, valid, visible);
        TerminateCurrentProcessForHostClose();
    }
}

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

    if (!InstallWindowProcedureAdapter(window) ||
        !SetWindowAttribute(window, GWL_STYLE, style) ||
        !SetWindowAttribute(window, GWL_EXSTYLE, kExtendedStyle) ||
        SetWindowTextA(window, kWindowTitle) == FALSE)
    {
        return false;
    }
    if (SetWindowPos(window,
                     HWND_TOP,
                     x,
                     y,
                     width,
                     height,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW) == FALSE)
    {
        return false;
    }
    return StartWindowLifetimeWatcher(window);
}
