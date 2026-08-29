#include "window_mode.h"

#include "host_window_shell.h"

#include <dwmapi.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#ifndef RE2DJ_VERSION
#define RE2DJ_VERSION "0.0.0"
#endif

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_fullscreen = FALSE;
extern "C" char g_re2dj_graphics_trace_path[MAX_PATH];

namespace
{

constexpr char kWindowTitleFormat[] =
    "re2DJ v%s - Build %s - SDL3 OpenGL - FPS : %.1f";
constexpr DWORD kWindowedScale = 2;
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD kFullscreenStyle = WS_POPUP;
constexpr DWORD kExtendedStyle = WS_EX_APPWINDOW;
constexpr DWORD kWindowLifetimePollMilliseconds = 50;
constexpr DWORD kWindowLifetimeSamplePolls = 20;
constexpr DWORD kWindowLifetimeMaximumSamples = 600;
PVOID volatile g_watched_window = nullptr;
volatile LONG g_window_lifetime_watcher_started = FALSE;
SRWLOCK g_window_title_lock = SRWLOCK_INIT;
char g_window_title[192] = {};
volatile LONG g_caption_trace_count = 0;

void CopyWindowTitle(char* destination, std::size_t destination_size)
{
    AcquireSRWLockShared(&g_window_title_lock);
    std::snprintf(destination, destination_size, "%s", g_window_title);
    ReleaseSRWLockShared(&g_window_title_lock);
}

void StoreWindowTitle(const char* title)
{
    AcquireSRWLockExclusive(&g_window_title_lock);
    std::snprintf(g_window_title, sizeof(g_window_title), "%s", title);
    ReleaseSRWLockExclusive(&g_window_title_lock);
}

void TraceWindowCaption(const char* event, HWND window)
{
    if (InterlockedIncrement(&g_caption_trace_count) > 32)
    {
        return;
    }
    char stored_title[192] = {};
    char actual_title[192] = {};
    CopyWindowTitle(stored_title, sizeof(stored_title));
    const LRESULT actual_length = window == nullptr
                                      ? 0
                                      : DefWindowProcA(window,
                                                       WM_GETTEXT,
                                                       sizeof(actual_title),
                                                       reinterpret_cast<LPARAM>(actual_title));
    const HICON icon = window == nullptr
                           ? nullptr
                           : reinterpret_cast<HICON>(
                                 DefWindowProcA(window, WM_GETICON, ICON_SMALL, 0));
    char line[384] = {};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "re2dj:hle:window-caption:event=%s:hwnd=%p:valid=%lu:stored-length=%zu:stored-prefix=%lu:actual-length=%ld:actual-prefix=%lu:icon=%lu:style=0x%08lx:exstyle=0x%08lx:wndproc=0x%08lx\r\n",
        event,
        reinterpret_cast<void*>(window),
        static_cast<unsigned long>(window != nullptr && IsWindow(window) != FALSE),
        std::strlen(stored_title),
        static_cast<unsigned long>(std::strncmp(stored_title, "re2DJ v", 7) == 0),
        static_cast<long>(actual_length),
        static_cast<unsigned long>(std::strncmp(actual_title, "re2DJ v", 7) == 0),
        static_cast<unsigned long>(icon != nullptr),
        static_cast<unsigned long>(window == nullptr ? 0 : GetWindowLongPtrA(window, GWL_STYLE)),
        static_cast<unsigned long>(window == nullptr ? 0 : GetWindowLongPtrA(window, GWL_EXSTYLE)),
        static_cast<unsigned long>(window == nullptr ? 0 : GetWindowLongPtrA(window, GWLP_WNDPROC)));
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

HWND PresentationWindow(HWND guest_window)
{
    const HWND host_window = ResolveRe2djHostWindow(guest_window);
    return host_window == nullptr ? guest_window : host_window;
}

void HandleHostClose(HWND guest_window)
{
    const HWND host_window = ResolveRe2djHostWindow(guest_window);
    TraceWindowLifetime("close-message",
                        host_window,
                        IsWindow(host_window),
                        IsWindowVisible(host_window));
    TerminateCurrentProcessForHostClose();
}

bool AdjustWindowBoundsForDpi(HWND window,
                              RECT* bounds,
                              DWORD style,
                              DWORD extended_style)
{
    using AdjustWindowRectExForDpiFunction =
        BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const HMODULE user32 = GetModuleHandleA("user32.dll");
    const auto adjust_for_dpi = user32 == nullptr
                                    ? nullptr
                                    : reinterpret_cast<AdjustWindowRectExForDpiFunction>(
                                          GetProcAddress(user32, "AdjustWindowRectExForDpi"));
    const UINT dpi = GetDpiForWindow(window);
    if (adjust_for_dpi != nullptr && dpi != 0)
    {
        return adjust_for_dpi(bounds, style, FALSE, extended_style, dpi) != FALSE;
    }
    return AdjustWindowRectEx(bounds, style, FALSE, extended_style) != FALSE;
}

}  // namespace

extern "C" __declspec(dllexport) void WINAPI Re2djExitIfWindowClosed(HWND window)
{
    window = PresentationWindow(window);
    const BOOL valid = window == nullptr ? FALSE : IsWindow(window);
    const BOOL visible = valid == FALSE ? FALSE : IsWindowVisible(window);
    if (window != nullptr && (valid == FALSE || visible == FALSE))
    {
        TraceWindowLifetime("flip-exit", window, valid, visible);
        TerminateCurrentProcessForHostClose();
    }
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djUpdateWindowTitle(HWND window, double fps)
{
    if (window == nullptr || IsWindow(window) == FALSE || !std::isfinite(fps) || fps < 0.0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    char title[192] = {};
    const int length = std::snprintf(
        title, sizeof(title), kWindowTitleFormat, RE2DJ_VERSION, __DATE__, fps);
    if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(title))
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    StoreWindowTitle(title);
    const HWND presentation_window = PresentationWindow(window);
    const BOOL updated = SetRe2djHostWindowTitle(presentation_window, title);
    TraceWindowCaption("title-update", presentation_window);
    return updated;
}

bool ApplyRe2djWindowMode(HWND window, DWORD client_width, DWORD client_height)
{
    if (window == nullptr || !IsWindow(window) || client_width == 0 || client_height == 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    if (g_re2dj_fullscreen == FALSE &&
        (client_width > static_cast<DWORD>((std::numeric_limits<LONG>::max)()) /
                            kWindowedScale ||
         client_height > static_cast<DWORD>((std::numeric_limits<LONG>::max)()) /
                             kWindowedScale))
    {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return false;
    }

    const HWND host_window = EnsureRe2djHostWindow(window, &HandleHostClose);
    if (host_window == nullptr)
    {
        return false;
    }
    const HMONITOR monitor = MonitorFromWindow(host_window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor == nullptr || GetMonitorInfoA(monitor, &monitor_info) == FALSE)
    {
        return false;
    }

    const bool fullscreen = g_re2dj_fullscreen != FALSE;
    const DWORD style = fullscreen ? kFullscreenStyle : kWindowedStyle;
    RECT bounds = fullscreen
                      ? monitor_info.rcMonitor
                      : RECT{0,
                             0,
                             static_cast<LONG>(client_width * kWindowedScale),
                             static_cast<LONG>(client_height * kWindowedScale)};
    if (!fullscreen && !AdjustWindowBoundsForDpi(host_window,
                                                 &bounds,
                                                 style,
                                                 kExtendedStyle))
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

    if (Re2djUpdateWindowTitle(window, 0.0) == FALSE)
    {
        return false;
    }
    if (!fullscreen)
    {
        SetRe2djHostWindowIcon(host_window);
    }
    if (!ConfigureRe2djHostWindow(host_window,
                                  window,
                                  style,
                                  kExtendedStyle,
                                  x,
                                  y,
                                  width,
                                  height))
    {
        return false;
    }
    DWMNCRENDERINGPOLICY non_client_policy = DWMNCRP_ENABLED;
    const HRESULT dwm_result = DwmSetWindowAttribute(host_window,
                                                     DWMWA_NCRENDERING_POLICY,
                                                     &non_client_policy,
                                                     sizeof(non_client_policy));
    if (SUCCEEDED(dwm_result))
    {
        RedrawWindow(host_window,
                     nullptr,
                     nullptr,
                     RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    }
    TraceWindowCaption("mode-applied", host_window);
    return StartWindowLifetimeWatcher(host_window);
}
