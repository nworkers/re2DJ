#include "host_window_shell.h"

namespace
{

constexpr char kHostWindowClassName[] = "re2dj.host-window-shell";
constexpr char kGuestWindowProperty[] = "re2dj.guest-window";
constexpr char kHostWindowProperty[] = "re2dj.host-window";
constexpr char kCloseCallbackProperty[] = "re2dj.host-close-callback";
constexpr DWORD kGuestStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
thread_local bool g_host_caption_update = false;

LRESULT CALLBACK HostWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    const HWND guest_window = reinterpret_cast<HWND>(GetPropA(window, kGuestWindowProperty));
    if ((message == WM_SETTEXT || message == WM_SETICON) && !g_host_caption_update)
    {
        return TRUE;
    }
    if (message == WM_SIZE && guest_window != nullptr && IsWindow(guest_window) != FALSE)
    {
        const int width = static_cast<int>(LOWORD(lparam));
        const int height = static_cast<int>(HIWORD(lparam));
        SetWindowPos(guest_window,
                     HWND_TOP,
                     0,
                     0,
                     width,
                     height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        return 0;
    }
    if (message == WM_CLOSE)
    {
        const auto close_callback = reinterpret_cast<Re2djHostCloseCallback>(
            reinterpret_cast<ULONG_PTR>(GetPropA(window, kCloseCallbackProperty)));
        if (close_callback != nullptr)
        {
            close_callback(guest_window);
        }
        return 0;
    }
    if (message == WM_NCDESTROY)
    {
        if (guest_window != nullptr && IsWindow(guest_window) != FALSE)
        {
            RemovePropA(guest_window, kHostWindowProperty);
        }
        RemovePropA(window, kGuestWindowProperty);
        RemovePropA(window, kCloseCallbackProperty);
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

HINSTANCE CurrentModule()
{
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&HostWindowProcedure),
                       &module);
    return module;
}

bool RegisterHostWindowClass(HINSTANCE module)
{
    WNDCLASSEXA existing = {};
    if (GetClassInfoExA(module, kHostWindowClassName, &existing) != FALSE)
    {
        return true;
    }
    WNDCLASSEXA window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = HostWindowProcedure;
    window_class.hInstance = module;
    window_class.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    window_class.hIconSm = window_class.hIcon;
    window_class.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    window_class.lpszClassName = kHostWindowClassName;
    return RegisterClassExA(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool SetWindowAttribute(HWND window, int index, LONG_PTR value)
{
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(window, index, value);
    return previous != 0 || GetLastError() == ERROR_SUCCESS;
}

}  // namespace

HWND EnsureRe2djHostWindow(HWND guest_window, Re2djHostCloseCallback close_callback)
{
    if (guest_window == nullptr || IsWindow(guest_window) == FALSE || close_callback == nullptr)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    const HWND existing = ResolveRe2djHostWindow(guest_window);
    if (existing != nullptr)
    {
        return existing;
    }
    const HINSTANCE module = CurrentModule();
    if (module == nullptr || !RegisterHostWindowClass(module))
    {
        return nullptr;
    }
    const HWND host_window = CreateWindowExA(WS_EX_APPWINDOW,
                                              kHostWindowClassName,
                                              "",
                                              WS_OVERLAPPEDWINDOW,
                                              CW_USEDEFAULT,
                                              CW_USEDEFAULT,
                                              640,
                                              480,
                                              nullptr,
                                              nullptr,
                                              module,
                                              nullptr);
    if (host_window == nullptr)
    {
        return nullptr;
    }
    if (SetPropA(host_window, kGuestWindowProperty, guest_window) == FALSE ||
        SetPropA(host_window,
                 kCloseCallbackProperty,
                 reinterpret_cast<HANDLE>(reinterpret_cast<ULONG_PTR>(close_callback))) == FALSE ||
        SetPropA(guest_window, kHostWindowProperty, host_window) == FALSE)
    {
        DestroyWindow(host_window);
        return nullptr;
    }
    ShowWindow(guest_window, SW_HIDE);
    if (!SetWindowAttribute(guest_window, GWL_STYLE, kGuestStyle) ||
        !SetWindowAttribute(guest_window, GWL_EXSTYLE, 0))
    {
        DestroyWindow(host_window);
        return nullptr;
    }
    SetLastError(ERROR_SUCCESS);
    const HWND previous_parent = SetParent(guest_window, host_window);
    const DWORD parent_error = GetLastError();
    if (previous_parent == nullptr && parent_error != ERROR_SUCCESS)
    {
        DestroyWindow(host_window);
        return nullptr;
    }
    return host_window;
}

HWND ResolveRe2djHostWindow(HWND guest_window)
{
    if (guest_window == nullptr)
    {
        return nullptr;
    }
    const HWND host_window = reinterpret_cast<HWND>(GetPropA(guest_window, kHostWindowProperty));
    return host_window != nullptr && IsWindow(host_window) != FALSE ? host_window : nullptr;
}

bool ConfigureRe2djHostWindow(HWND host_window,
                              HWND guest_window,
                              DWORD host_style,
                              DWORD host_extended_style,
                              int x,
                              int y,
                              int width,
                              int height)
{
    if (host_window == nullptr || guest_window == nullptr ||
        IsWindow(host_window) == FALSE || IsWindow(guest_window) == FALSE)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    if (!SetWindowAttribute(host_window, GWL_STYLE, host_style) ||
        !SetWindowAttribute(host_window, GWL_EXSTYLE, host_extended_style) ||
        SetWindowPos(host_window,
                     HWND_TOP,
                     x,
                     y,
                     width,
                     height,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW) == FALSE)
    {
        return false;
    }
    RECT client = {};
    if (GetClientRect(host_window, &client) == FALSE)
    {
        return false;
    }
    return SetWindowPos(guest_window,
                        HWND_TOP,
                        0,
                        0,
                        client.right - client.left,
                        client.bottom - client.top,
                        SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
}

BOOL SetRe2djHostWindowTitle(HWND host_window, const char* title)
{
    if (host_window == nullptr || title == nullptr)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    g_host_caption_update = true;
    const BOOL updated = SetWindowTextA(host_window, title);
    g_host_caption_update = false;
    return updated;
}

void SetRe2djHostWindowIcon(HWND host_window)
{
    const HICON application_icon = LoadIconA(nullptr, IDI_APPLICATION);
    if (host_window == nullptr || application_icon == nullptr)
    {
        return;
    }
    g_host_caption_update = true;
    SendMessageA(host_window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(application_icon));
    SendMessageA(host_window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(application_icon));
    g_host_caption_update = false;
}
