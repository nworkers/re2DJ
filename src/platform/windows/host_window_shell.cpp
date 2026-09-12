#include "host_window_shell.h"

#include "window_mode.h"

#include <cstdlib>
#include <new>

namespace
{

constexpr char kHostWindowClassName[] = "re2dj.host-window-shell";
constexpr char kGuestWindowProperty[] = "re2dj.guest-window";
constexpr char kHostWindowProperty[] = "re2dj.host-window";
constexpr char kCloseCallbackProperty[] = "re2dj.host-close-callback";
constexpr char kGuestWindowProcedureProperty[] = "re2dj.guest-window-procedure";
constexpr char kHostWindowInputProperty[] = "re2dj.host-window-input";
constexpr DWORD kGuestStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
constexpr LONG kMaximumHostTrackDimension = 32767;
constexpr LONG kMinimumHostClientWidth = 320;
constexpr LONG kMinimumHostClientHeight = 240;
constexpr LONG kAspectWidth = 4;
constexpr LONG kAspectHeight = 3;
thread_local bool g_host_caption_update = false;

struct ClickState
{
    DWORD last_click_tick = 0;
    POINT last_click_point = {};
    bool has_last_click = false;
};

struct GuestWindowState
{
    WNDPROC original_procedure = nullptr;
    ClickState click;
};

struct HostWindowInputState
{
    ClickState click;
};

LRESULT CALLBACK GuestWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
bool GetWindowFrameExtents(HWND window, LONG* horizontal, LONG* vertical);
bool AdjustSizingRectangleToAspect(HWND window, WPARAM sizing_edge, RECT* rectangle);

GuestWindowState* GuestState(HWND window)
{
    return reinterpret_cast<GuestWindowState*>(
        GetPropA(window, kGuestWindowProcedureProperty));
}

void RestoreGuestWindowProcedure(HWND window)
{
    GuestWindowState* const state = GuestState(window);
    if (state == nullptr)
    {
        return;
    }
    const LONG_PTR current_procedure = IsWindow(window) != FALSE
                                           ? GetWindowLongPtrA(window, GWLP_WNDPROC)
                                           : 0;
    if (current_procedure == reinterpret_cast<LONG_PTR>(&GuestWindowProcedure) &&
        state->original_procedure != nullptr)
    {
        SetWindowLongPtrA(window,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(state->original_procedure));
    }
    RemovePropA(window, kGuestWindowProcedureProperty);
    delete state;
}

DWORD ScaleForKey(WPARAM key)
{
    switch (key)
    {
    case '1':
    case VK_NUMPAD1:
        return 1;
    case '2':
    case VK_NUMPAD2:
        return 2;
    case '3':
    case VK_NUMPAD3:
        return 3;
    default:
        return 0;
    }
}

bool IsDoubleClick(ClickState* state, LPARAM lparam)
{
    if (state == nullptr)
    {
        return false;
    }
    const POINT point = {static_cast<short>(LOWORD(lparam)),
                         static_cast<short>(HIWORD(lparam))};
    const DWORD now = GetTickCount();
    const int delta_x = std::abs(point.x - state->last_click_point.x);
    const int delta_y = std::abs(point.y - state->last_click_point.y);
    const bool double_click =
        state->has_last_click && now - state->last_click_tick <= GetDoubleClickTime() &&
        delta_x <= GetSystemMetrics(SM_CXDOUBLECLK) &&
        delta_y <= GetSystemMetrics(SM_CYDOUBLECLK);
    state->last_click_tick = now;
    state->last_click_point = point;
    state->has_last_click = !double_click;
    return double_click;
}

bool HandleScaleShortcut(HWND guest_window, UINT message, WPARAM wparam, LPARAM lparam)
{
    const bool alt_key_message =
        message == WM_SYSKEYDOWN ||
        (message == WM_KEYDOWN && (GetKeyState(VK_MENU) & 0x8000) != 0);
    if (!alt_key_message || (static_cast<ULONG_PTR>(lparam) & (1u << 30)) != 0)
    {
        return false;
    }
    const DWORD scale = ScaleForKey(wparam);
    return scale != 0 && SetRe2djWindowScale(guest_window, scale);
}

LRESULT CALLBACK GuestWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    GuestWindowState* const state = GuestState(window);
    if (state == nullptr || state->original_procedure == nullptr)
    {
        return DefWindowProcA(window, message, wparam, lparam);
    }

    if (HandleScaleShortcut(window, message, wparam, lparam))
    {
        return 0;
    }
    if (message == WM_LBUTTONDBLCLK)
    {
        state->click.has_last_click = false;
        if (ToggleRe2djFullscreen(window))
        {
            return 0;
        }
    }
    else if (message == WM_LBUTTONDOWN && IsDoubleClick(&state->click, lparam) &&
             ToggleRe2djFullscreen(window))
    {
        return 0;
    }

    const LRESULT result = CallWindowProcA(
        state->original_procedure, window, message, wparam, lparam);
    if (message == WM_NCDESTROY)
    {
        RestoreGuestWindowProcedure(window);
    }
    return result;
}

bool SubclassGuestWindow(HWND window)
{
    auto* const state = new (std::nothrow) GuestWindowState;
    if (state == nullptr)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(
        window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&GuestWindowProcedure));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS)
    {
        delete state;
        return false;
    }
    state->original_procedure = reinterpret_cast<WNDPROC>(previous);
    if (state->original_procedure == nullptr ||
        SetPropA(window,
                 kGuestWindowProcedureProperty,
                 reinterpret_cast<HANDLE>(reinterpret_cast<ULONG_PTR>(state))) == FALSE)
    {
        if (state->original_procedure != nullptr)
        {
            SetWindowLongPtrA(window,
                              GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(state->original_procedure));
        }
        delete state;
        return false;
    }
    return true;
}

bool EnsureGuestWindowProcedure(HWND window)
{
    GuestWindowState* const state = GuestState(window);
    if (state == nullptr)
    {
        return SubclassGuestWindow(window);
    }
    if (GetWindowLongPtrA(window, GWLP_WNDPROC) ==
        reinterpret_cast<LONG_PTR>(&GuestWindowProcedure))
    {
        return true;
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(
        window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&GuestWindowProcedure));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS)
    {
        return false;
    }
    state->original_procedure = reinterpret_cast<WNDPROC>(previous);
    state->click.has_last_click = false;
    return state->original_procedure != nullptr;
}

LRESULT CALLBACK HostWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    const HWND guest_window = reinterpret_cast<HWND>(GetPropA(window, kGuestWindowProperty));
    if ((message == WM_SETTEXT || message == WM_SETICON) && !g_host_caption_update)
    {
        return TRUE;
    }
    if (guest_window != nullptr &&
        HandleScaleShortcut(guest_window, message, wparam, lparam))
    {
        return 0;
    }
    if (guest_window != nullptr && message == WM_LBUTTONDBLCLK &&
        ToggleRe2djFullscreen(guest_window))
    {
        auto* const input_state = reinterpret_cast<HostWindowInputState*>(
            GetPropA(window, kHostWindowInputProperty));
        if (input_state != nullptr)
        {
            input_state->click.has_last_click = false;
        }
        return 0;
    }
    if (guest_window != nullptr && message == WM_LBUTTONDOWN)
    {
        auto* const input_state = reinterpret_cast<HostWindowInputState*>(
            GetPropA(window, kHostWindowInputProperty));
        if (input_state != nullptr && IsDoubleClick(&input_state->click, lparam) &&
            ToggleRe2djFullscreen(guest_window))
        {
            return 0;
        }
    }
    if (message == WM_GETMINMAXINFO && lparam != 0)
    {
        const LRESULT result = DefWindowProcA(window, message, wparam, lparam);
        auto* const limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMaxTrackSize.x = kMaximumHostTrackDimension;
        limits->ptMaxTrackSize.y = kMaximumHostTrackDimension;
        LONG frame_width = 0;
        LONG frame_height = 0;
        if (GetWindowFrameExtents(window, &frame_width, &frame_height))
        {
            const LONG minimum_width = kMinimumHostClientWidth + frame_width;
            const LONG minimum_height = kMinimumHostClientHeight + frame_height;
            if (limits->ptMinTrackSize.x < minimum_width)
            {
                limits->ptMinTrackSize.x = minimum_width;
            }
            if (limits->ptMinTrackSize.y < minimum_height)
            {
                limits->ptMinTrackSize.y = minimum_height;
            }
        }
        return result;
    }
    if (message == WM_SIZING && lparam != 0 &&
        AdjustSizingRectangleToAspect(window,
                                      wparam,
                                      reinterpret_cast<RECT*>(lparam)))
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
            RestoreGuestWindowProcedure(guest_window);
            RemovePropA(guest_window, kHostWindowProperty);
        }
        RemovePropA(window, kGuestWindowProperty);
        RemovePropA(window, kCloseCallbackProperty);
        auto* const input_state = reinterpret_cast<HostWindowInputState*>(
            RemovePropA(window, kHostWindowInputProperty));
        delete input_state;
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
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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

bool GetWindowFrameExtents(HWND window, LONG* horizontal, LONG* vertical)
{
    if (window == nullptr || horizontal == nullptr || vertical == nullptr)
    {
        return false;
    }
    RECT window_rectangle = {};
    RECT client_rectangle = {};
    POINT client_origin = {};
    if (GetWindowRect(window, &window_rectangle) == FALSE ||
        GetClientRect(window, &client_rectangle) == FALSE ||
        ClientToScreen(window, &client_origin) == FALSE)
    {
        return false;
    }
    const LONG client_width = client_rectangle.right - client_rectangle.left;
    const LONG client_height = client_rectangle.bottom - client_rectangle.top;
    const LONG window_width = window_rectangle.right - window_rectangle.left;
    const LONG window_height = window_rectangle.bottom - window_rectangle.top;
    if (client_width <= 0 || client_height <= 0 || window_width < client_width ||
        window_height < client_height)
    {
        return false;
    }
    *horizontal = window_width - client_width;
    *vertical = window_height - client_height;
    return true;
}

bool AdjustSizingRectangleToAspect(HWND window, WPARAM sizing_edge, RECT* rectangle)
{
    if (rectangle == nullptr || (GetWindowLongPtrA(window, GWL_STYLE) & WS_POPUP) != 0)
    {
        return false;
    }
    LONG frame_width = 0;
    LONG frame_height = 0;
    if (!GetWindowFrameExtents(window, &frame_width, &frame_height))
    {
        return false;
    }
    const LONG outer_width = rectangle->right - rectangle->left;
    const LONG outer_height = rectangle->bottom - rectangle->top;
    const LONG client_width = outer_width - frame_width;
    const LONG client_height = outer_height - frame_height;
    if (client_width <= 0 || client_height <= 0)
    {
        return false;
    }

    const bool resize_left = sizing_edge == WMSZ_LEFT || sizing_edge == WMSZ_TOPLEFT ||
                             sizing_edge == WMSZ_BOTTOMLEFT;
    const bool resize_right = sizing_edge == WMSZ_RIGHT || sizing_edge == WMSZ_TOPRIGHT ||
                              sizing_edge == WMSZ_BOTTOMRIGHT;
    const bool resize_top = sizing_edge == WMSZ_TOP || sizing_edge == WMSZ_TOPLEFT ||
                            sizing_edge == WMSZ_TOPRIGHT;
    const bool resize_bottom = sizing_edge == WMSZ_BOTTOM || sizing_edge == WMSZ_BOTTOMLEFT ||
                               sizing_edge == WMSZ_BOTTOMRIGHT;
    const bool resize_horizontally = resize_left || resize_right;
    const bool resize_vertically = resize_top || resize_bottom;
    if (!resize_horizontally && !resize_vertically)
    {
        return false;
    }

    LONG adjusted_client_width = client_width;
    LONG adjusted_client_height = client_height;
    if (resize_horizontally && !resize_vertically)
    {
        adjusted_client_height = (client_width * kAspectHeight + kAspectWidth / 2) /
                                 kAspectWidth;
    }
    else if (!resize_horizontally && resize_vertically)
    {
        adjusted_client_width = (client_height * kAspectWidth + kAspectHeight / 2) /
                                kAspectHeight;
    }
    else
    {
        const LONG width_from_height =
            (client_height * kAspectWidth + kAspectHeight / 2) / kAspectHeight;
        const LONG height_from_width =
            (client_width * kAspectHeight + kAspectWidth / 2) / kAspectWidth;
        const LONG width_delta = std::abs(width_from_height - client_width);
        const LONG height_delta = std::abs(height_from_width - client_height);
        if (width_delta <= height_delta)
        {
            adjusted_client_width = width_from_height;
        }
        else
        {
            adjusted_client_height = height_from_width;
        }
    }
    const LONG adjusted_outer_width = adjusted_client_width + frame_width;
    const LONG adjusted_outer_height = adjusted_client_height + frame_height;
    if (resize_left)
    {
        rectangle->left = rectangle->right - adjusted_outer_width;
    }
    else if (resize_right)
    {
        rectangle->right = rectangle->left + adjusted_outer_width;
    }
    if (resize_top)
    {
        rectangle->top = rectangle->bottom - adjusted_outer_height;
    }
    else if (resize_bottom)
    {
        rectangle->bottom = rectangle->top + adjusted_outer_height;
    }
    if (resize_horizontally && !resize_vertically)
    {
        const LONG center_y = (rectangle->top + rectangle->bottom) / 2;
        rectangle->top = center_y - adjusted_outer_height / 2;
        rectangle->bottom = rectangle->top + adjusted_outer_height;
    }
    else if (!resize_horizontally && resize_vertically)
    {
        const LONG center_x = (rectangle->left + rectangle->right) / 2;
        rectangle->left = center_x - adjusted_outer_width / 2;
        rectangle->right = rectangle->left + adjusted_outer_width;
    }
    return true;
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
        return EnsureGuestWindowProcedure(guest_window) ? existing : nullptr;
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
    auto* const input_state = new (std::nothrow) HostWindowInputState;
    if (input_state == nullptr ||
        SetPropA(host_window,
                 kHostWindowInputProperty,
                 reinterpret_cast<HANDLE>(reinterpret_cast<ULONG_PTR>(input_state))) == FALSE)
    {
        delete input_state;
        DestroyWindow(host_window);
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
    if (!EnsureGuestWindowProcedure(guest_window))
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

bool SuspendRe2djGuestWindowInput(HWND guest_window)
{
    if (guest_window == nullptr || IsWindow(guest_window) == FALSE)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    GuestWindowState* const state = GuestState(guest_window);
    if (state == nullptr ||
        GetWindowLongPtrA(guest_window, GWLP_WNDPROC) !=
            reinterpret_cast<LONG_PTR>(&GuestWindowProcedure))
    {
        return true;
    }
    if (state->original_procedure == nullptr)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrA(
        guest_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(state->original_procedure));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS)
    {
        return false;
    }
    state->click.has_last_click = false;
    return true;
}

bool EnsureRe2djGuestWindowInput(HWND guest_window)
{
    if (guest_window == nullptr || IsWindow(guest_window) == FALSE)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return EnsureGuestWindowProcedure(guest_window);
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
    // Keep the host observable while changing its frame and position.
    const DWORD visible_host_style = host_style | WS_VISIBLE;
    if (!SetWindowAttribute(host_window, GWL_STYLE, visible_host_style) ||
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
