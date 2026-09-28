#include "re2dj/hle/modules/user32_module.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gdi_bitmaps.h"
#include "re2dj/hle/gdi_raster.h"
#include "re2dj/hle/guest_font.h"
#include "re2dj/hle/guest_gdi.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/host_presentation.h"
#include "re2dj/hle/win32_errors.h"
#include "re2dj/hle/wsprintf.h"
#include "re2dj/hle/modules/gdi32_module.h"
#include "re2dj/hle/modules/resolve_only_modules.h"

namespace re2dj::hle::modules
{
namespace
{

// The calling thread's active window, NULL before one is activated.
bool GetActiveWindow(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = "user32 result is null";
        }
        return false;
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process != nullptr)
    {
        result->eax = process->user().active_window();
    }
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// The foreground window. The guest is the host's foreground application and
// has one thread, so its foreground window is that thread's active window.
bool GetForegroundWindow(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return GetActiveWindow(call, result, error);
}

// MessageBoxA(hWnd, lpText, lpCaption, uType). No platform service shows the
// box yet, so the result is the default button's ID, as if the user accepted
// it; see MessageBoxDefaultButton.
bool MessageBoxA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        if (error != nullptr)
        {
            *error = result == nullptr ? "user32 result is null"
                                       : "user32 MessageBoxA argument shape is invalid";
        }
        return false;
    }
    *result = {};
    result->eax = MessageBoxDefaultButton(call.arguments[3]);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// SetTimer(hWnd, nIDEvent, uElapse, lpTimerFunc) for thread timers only:
// no export creates a window, so a window timer cannot be asked for.
bool SetTimer(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        if (error != nullptr)
        {
            *error = result == nullptr ? "user32 result is null"
                                       : "user32 SetTimer argument shape is invalid";
        }
        return false;
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr || call.arguments[0] != 0)
    {
        if (error != nullptr)
        {
            *error = process == nullptr ? "user32 SetTimer needs the guest process"
                                        : "user32 SetTimer has no window service for a window timer";
        }
        return false;
    }
    GuestClockReading clock;
    if (!call.services->ReadClock(&clock))
    {
        if (error != nullptr)
        {
            *error = "user32 SetTimer needs the host clock";
        }
        return false;
    }
    result->eax = process->SetThreadTimer(call.arguments[1], call.arguments[2], call.arguments[3], clock.tick_ms);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

bool Fail(std::string* error, std::string text)
{
    if (error != nullptr)
    {
        *error = std::move(text);
    }
    return false;
}

bool Succeed(std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// A resource name argument: an ID when the high word is zero (MAKEINTRESOURCE).
bool IsIntResource(std::uint32_t name)
{
    return (name & 0xFFFF0000U) == 0;
}

// LoadIconA(hInstance, lpIconName) and LoadCursorA(hInstance, lpCursorName)
// for the system set only (hInstance NULL). Measured on Windows 11: an ID or
// name the system lacks gives NULL with ERROR_RESOURCE_TYPE_NOT_FOUND for
// icons and ERROR_RESOURCE_NAME_NOT_FOUND for cursors; a system ID gives the
// same shared handle on every call. Module resources are not modelled.
bool LoadSystemImage(const ImportCall& call,
                     ImportReturn* result,
                     std::string* error,
                     bool icon)
{
    const char* name = icon ? "LoadIconA" : "LoadCursorA";
    if (result == nullptr || call.arguments.size() != 2)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : std::string("user32 ") + name + " argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, std::string("user32 ") + name + " needs the guest process");
    }
    if (call.arguments[0] != 0)
    {
        return Fail(error, std::string("user32 ") + name + " has no model of module resources");
    }
    const std::uint32_t resource = call.arguments[1];
    GuestUser& user = process->user();
    result->eax = !IsIntResource(resource) ? 0
                  : icon                   ? user.SystemIcon(resource)
                                           : user.SystemCursor(resource);
    if (result->eax == 0)
    {
        call.services->SetLastError(icon ? kWin32ErrorResourceTypeNotFound
                                         : kWin32ErrorResourceNameNotFound);
    }
    return Succeed(error);
}

bool LoadIconA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return LoadSystemImage(call, result, error, true);
}

bool LoadCursorA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return LoadSystemImage(call, result, error, false);
}

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

// The module a class belongs to: a NULL hInstance means the main image, as
// Windows 11 finds a class registered with NULL from CreateWindowExA with
// either NULL or the executable's handle.
std::uint32_t ClassInstance(const GuestProcess& process, std::uint32_t instance)
{
    return instance != 0 ? instance : process.image_base();
}

// RegisterClassA(lpWndClass): records the class and returns its atom. A
// second class of the same name for the instance fails with
// ERROR_CLASS_ALREADY_EXISTS. An atom as the class name is not modelled.
bool RegisterClassA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 RegisterClassA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 RegisterClassA needs the guest process");
    }
    // WNDCLASSA: style, lpfnWndProc, cbClsExtra, cbWndExtra, hInstance,
    // hIcon, hCursor, hbrBackground, lpszMenuName, lpszClassName.
    std::array<std::uint8_t, 40> bytes{};
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &read_error))
    {
        return Fail(error, "user32 RegisterClassA cannot read WNDCLASSA: " + read_error);
    }
    GuestWindowClass window_class;
    window_class.style = ReadU32(&bytes[0]);
    window_class.window_procedure = ReadU32(&bytes[4]);
    window_class.class_extra = ReadU32(&bytes[8]);
    window_class.window_extra = ReadU32(&bytes[12]);
    window_class.instance = ClassInstance(*process, ReadU32(&bytes[16]));
    window_class.icon = ReadU32(&bytes[20]);
    window_class.cursor = ReadU32(&bytes[24]);
    window_class.background = ReadU32(&bytes[28]);
    window_class.menu_name = ReadU32(&bytes[32]);
    const std::uint32_t class_name = ReadU32(&bytes[36]);
    if (IsIntResource(class_name))
    {
        return Fail(error, "user32 RegisterClassA has no model of an atom class name");
    }
    if (!call.services->ReadGuestString(runtime::GuestAddress(class_name), &window_class.name, &read_error))
    {
        return Fail(error, "user32 RegisterClassA cannot read the class name: " + read_error);
    }
    result->eax = process->user().AddClass(std::move(window_class));
    if (result->eax == 0)
    {
        call.services->SetLastError(kWin32ErrorClassAlreadyExists);
    }
    return Succeed(error);
}

// winuser.h window messages the window model sends or answers.
constexpr std::uint32_t kWmCreate = 0x0001;
constexpr std::uint32_t kWmMove = 0x0003;
constexpr std::uint32_t kWmSize = 0x0005;
constexpr std::uint32_t kWmActivate = 0x0006;
constexpr std::uint32_t kWmSetFocus = 0x0007;
constexpr std::uint32_t kWmKillFocus = 0x0008;
constexpr std::uint32_t kWmPaint = 0x000F;
constexpr std::uint32_t kWmEraseBackground = 0x0014;
constexpr std::uint32_t kWmShowWindow = 0x0018;
constexpr std::uint32_t kWmActivateApp = 0x001C;
constexpr std::uint32_t kWmWindowPosChanging = 0x0046;
constexpr std::uint32_t kWmWindowPosChanged = 0x0047;
constexpr std::uint32_t kWmNcCreate = 0x0081;
constexpr std::uint32_t kWmNcCalcSize = 0x0083;
constexpr std::uint32_t kWmNcPaint = 0x0085;
constexpr std::uint32_t kWmNcActivate = 0x0086;

constexpr std::uint32_t kWsPopup = 0x80000000U;
constexpr std::uint32_t kWsVisible = 0x10000000U;
constexpr std::uint32_t kWsClipSiblings = 0x04000000U;
// A thin border, one pixel (SM_CXBORDER) on each side, as EZ2Dancer 2nd MOVE
// gives its WS_POPUP window.
constexpr std::uint32_t kWsBorder = 0x00800000U;
// Styles whose geometry or ownership the model does not have: WS_CHILD,
// WS_MINIMIZE, WS_MAXIMIZE, WS_DLGFRAME (and so WS_CAPTION), and
// WS_THICKFRAME.
constexpr std::uint32_t kWsUnmodelled = 0x40000000U | 0x20000000U | 0x01000000U | 0x00400000U | 0x00040000U;
constexpr std::uint32_t kWsExAppWindow = 0x00040000U;
constexpr std::uint32_t kCwUseDefault = 0x80000000U;
constexpr std::uint32_t kWaInactive = 0;
constexpr std::uint32_t kWaActive = 1;

// SetWindowPos flags as Windows 11 passes them while showing a new window:
// SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW, then SWP_NOSIZE | SWP_NOMOVE for
// the activation, and in WM_WINDOWPOSCHANGED also SWP_NOCLIENTSIZE,
// SWP_NOCLIENTMOVE, and an internal bit (0x10000000).
constexpr std::uint32_t kSwpNoSize = 0x0001;
constexpr std::uint32_t kSwpNoMove = 0x0002;
constexpr std::uint32_t kShowChangingFlags = 0x00000043U;
constexpr std::uint32_t kActivateChangingFlags = 0x00000003U;
constexpr std::uint32_t kShowChangedFlags = 0x10001843U;

void PutU32(std::vector<std::uint8_t>* bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        bytes->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint32_t PackPoint(std::int32_t low, std::int32_t high)
{
    return (static_cast<std::uint32_t>(low) & 0xFFFFU) | (static_cast<std::uint32_t>(high) << 16);
}

// Sends a message to the window's procedure: a guest call of
// WndProc(hwnd, message, wParam, lParam), with data as lParam when given.
bool SendToWindow(const ImportCall& call,
                  const GuestWindow& window,
                  std::uint32_t message,
                  std::uint32_t w_param,
                  std::uint32_t l_param,
                  std::vector<std::uint8_t>* data,
                  std::uint32_t* result,
                  std::string* error)
{
    GuestCall guest_call;
    guest_call.function = window.window_procedure;
    guest_call.arguments = {window.handle, message, w_param, l_param};
    if (data != nullptr)
    {
        guest_call.data = std::move(*data);
        guest_call.data_argument = 3;
    }
    std::string call_error;
    if (!call.services->CallGuest(&guest_call, result, &call_error))
    {
        char text[96];
        std::snprintf(text, sizeof(text), "user32 cannot send message %04x: ", message);
        return Fail(error, text + call_error);
    }
    if (data != nullptr)
    {
        *data = std::move(guest_call.data);
    }
    return true;
}

// WINDOWPOS {hwnd, hwndInsertAfter, x, y, cx, cy, flags}. With no other
// window in the guest's world, the new window goes in front (HWND_TOP).
std::vector<std::uint8_t> WindowPos(const GuestWindow& window, bool with_rect, std::uint32_t flags)
{
    std::vector<std::uint8_t> bytes;
    PutU32(&bytes, window.handle);
    PutU32(&bytes, 0);
    PutU32(&bytes, with_rect ? static_cast<std::uint32_t>(window.x) : 0);
    PutU32(&bytes, with_rect ? static_cast<std::uint32_t>(window.y) : 0);
    PutU32(&bytes, with_rect ? static_cast<std::uint32_t>(window.width) : 0);
    PutU32(&bytes, with_rect ? static_cast<std::uint32_t>(window.height) : 0);
    PutU32(&bytes, flags);
    return bytes;
}

// Makes window the focus window, sending WM_KILLFOCUS to the old one and
// WM_SETFOCUS to the new one, as SetFocus does.
bool SetFocusTo(const ImportCall& call, GuestUser& user, std::uint32_t window, std::string* error)
{
    const std::uint32_t previous = user.focus_window();
    if (previous == window)
    {
        return true;
    }
    std::uint32_t ignored = 0;
    user.set_focus_window(window);
    if (previous != 0)
    {
        GuestWindow* old_window = user.LookupWindow(previous);
        if (old_window != nullptr &&
            !SendToWindow(call, *old_window, kWmKillFocus, window, 0, nullptr, &ignored, error))
        {
            return false;
        }
    }
    GuestWindow* new_window = user.LookupWindow(window);
    return new_window == nullptr ||
           SendToWindow(call, *new_window, kWmSetFocus, previous, 0, nullptr, &ignored, error);
}

// ShowWindow(SW_SHOW) of a hidden top-level window, as Windows 11 sends it
// (WOW64), whether from CreateWindowEx with WS_VISIBLE or a later
// ShowWindow: shown, brought to the front, and activated with the keyboard
// focus; its whole client area is left to paint. The IME and accessibility
// messages Windows adds are left out. Another window being active, which
// Windows deactivates with messages of its own, is not modelled and stops.
bool ShowHiddenWindow(const ImportCall& call, GuestUser& user, std::uint32_t handle, std::string* error)
{
    if (user.active_window() != 0 && user.active_window() != handle)
    {
        return Fail(error, "user32 has no model of showing a window while another is active");
    }
    user.LookupWindow(handle)->style |= kWsVisible;
    user.LookupWindow(handle)->needs_paint = true;
    const GuestWindow shown = *user.LookupWindow(handle);
    std::uint32_t answer = 0;
    if (!SendToWindow(call, shown, kWmShowWindow, 1, 0, nullptr, &answer, error))
    {
        return false;
    }
    std::vector<std::uint8_t> data = WindowPos(shown, false, kShowChangingFlags);
    if (!SendToWindow(call, shown, kWmWindowPosChanging, 0, 0, &data, &answer, error))
    {
        return false;
    }
    data = WindowPos(shown, false, kActivateChangingFlags);
    if (!SendToWindow(call, shown, kWmWindowPosChanging, 0, 0, &data, &answer, error))
    {
        return false;
    }
    const std::uint32_t previous_active = user.active_window();
    user.set_active_window(handle);
    // WM_ACTIVATEAPP's lParam names the thread of the application losing
    // activation; no other application exists here.
    if (!SendToWindow(call, shown, kWmActivateApp, 1, 0, nullptr, &answer, error) ||
        !SendToWindow(call, shown, kWmNcActivate, 1, 0, nullptr, &answer, error) ||
        !SendToWindow(call, shown, kWmActivate, kWaActive, previous_active, nullptr, &answer, error) ||
        !SendToWindow(call, shown, kWmNcPaint, 1, 0, nullptr, &answer, error) ||
        !SendToWindow(call, shown, kWmEraseBackground, shown.device_context, 0, nullptr, &answer, error))
    {
        return false;
    }
    data = WindowPos(shown, true, kShowChangedFlags);
    return SendToWindow(call, shown, kWmWindowPosChanged, 0, 0, &data, &answer, error);
}

// ShowWindow(hWnd, nCmdShow) for SW_SHOW, as measured on Windows 11: a hidden
// window is shown (ShowHiddenWindow), 0 is returned, and the last error
// becomes 0; a window already visible gets no message and 24, the last error
// left alone. An unknown window is 0 with ERROR_INVALID_WINDOW_HANDLE. Other
// commands stop.
bool ShowWindow(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kSwShow = 5;
    constexpr std::uint32_t kAlreadyVisible = 24;
    if (result == nullptr || call.arguments.size() != 2)
    {
        return Fail(error, result == nullptr ? "user32 result is null" : "user32 ShowWindow argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 ShowWindow needs the guest process");
    }
    if (call.arguments[1] != kSwShow)
    {
        return Fail(error, "user32 ShowWindow has no model of this show command");
    }
    GuestUser& user = process->user();
    const GuestWindow* window = user.LookupWindow(call.arguments[0]);
    if (window == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidWindowHandle);
        return Succeed(error);
    }
    if ((window->style & kWsVisible) != 0)
    {
        result->eax = kAlreadyVisible;
        return Succeed(error);
    }
    if (!ShowHiddenWindow(call, user, call.arguments[0], error))
    {
        return false;
    }
    call.services->SetLastError(kWin32ErrorSuccess);
    return Succeed(error);
}

// EnumDisplaySettingsA(lpszDeviceName, iModeNum, lpDevMode) for the default
// device's current (ENUM_CURRENT_SETTINGS) or registry (ENUM_REGISTRY_
// SETTINGS) mode: the host desktop's mode, which is what the Windows
// product's guest reads from Windows. The DEVMODEA bytes written are the ones
// Windows 11 writes (measured): the device name "CDD", both versions 0x0401,
// dmSize 124, no driver extra, dmFields 0x207C00A0, zeros from dmPosition
// through the first byte of dmFormName, and the mode from dmBitsPerPel
// through dmDisplayFrequency; the rest is left alone. TRUE, the last error
// left alone. A named device and enumeration by index stop.
bool EnumDisplaySettingsA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kEnumCurrentSettings = 0xFFFFFFFFU;
    constexpr std::uint32_t kEnumRegistrySettings = 0xFFFFFFFEU;
    if (result == nullptr || call.arguments.size() != 3)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 EnumDisplaySettingsA argument shape is invalid");
    }
    *result = {};
    if (call.arguments[0] != 0 ||
        (call.arguments[1] != kEnumCurrentSettings && call.arguments[1] != kEnumRegistrySettings))
    {
        return Fail(error, "user32 EnumDisplaySettingsA has no model of a named device or a mode index");
    }
    HostPresentation* presentation = call.services == nullptr ? nullptr : call.services->Presentation();
    HostDisplayMode mode;
    std::string host_error = "no host presentation";
    if (presentation == nullptr || !presentation->DesktopDisplayMode(&mode, &host_error))
    {
        return Fail(error, "user32 EnumDisplaySettingsA needs the host desktop mode: " + host_error);
    }
    const std::uint32_t dev_mode = call.arguments[2];
    std::vector<std::uint8_t> head = {'C', 'D', 'D', 0};
    std::vector<std::uint8_t> versions;
    PutU32(&versions, 0x04010401U);  // dmSpecVersion, dmDriverVersion
    PutU32(&versions, 124);          // dmSize, dmDriverExtra 0
    PutU32(&versions, 0x207C00A0U);  // dmFields
    versions.resize(versions.size() + 27, 0);
    std::vector<std::uint8_t> fields;
    PutU32(&fields, mode.bits_per_pixel);
    PutU32(&fields, mode.width);
    PutU32(&fields, mode.height);
    PutU32(&fields, 0);  // dmDisplayFlags
    PutU32(&fields, mode.refresh_hz);
    std::string memory_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(dev_mode), head, &memory_error) ||
        !call.services->WriteGuestBytes(runtime::GuestAddress(dev_mode + 32), versions, &memory_error) ||
        !call.services->WriteGuestBytes(runtime::GuestAddress(dev_mode + 104), fields, &memory_error))
    {
        return Fail(error, "user32 EnumDisplaySettingsA cannot write the DEVMODEA: " + memory_error);
    }
    result->eax = 1;
    return Succeed(error);
}

// ChangeDisplaySettingsExA(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam),
// absorbed as the Windows product absorbs it (display_mode_boundary.cpp): the
// host display never changes, the guest's window is sized by the window
// policy instead, and DISP_CHANGE_SUCCESSFUL with the last error 0 is the
// answer that lets the game go on (1st SE quits on a failure).
bool ChangeDisplaySettingsExA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 5)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 ChangeDisplaySettingsExA argument shape is invalid");
    }
    *result = {};
    call.services->SetLastError(kWin32ErrorSuccess);
    return Succeed(error);
}

// CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y,
// nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) for a top-level
// WS_POPUP window without a menu, the shape 4th creates, with or without
// WS_BORDER (EZ2Dancer 2nd MOVE): the border changes only the client area
// DefWindowProc's WM_NCCALCSIZE gives, and so WM_SIZE and WM_MOVE. The messages and
// their arguments follow what Windows 11 sends such a window (WOW64):
// WM_NCCREATE, WM_NCCALCSIZE, WM_CREATE, WM_SIZE, WM_MOVE, and for
// WS_VISIBLE the show and activation sequence (ShowHiddenWindow). Messages Windows adds from
// other components (IME, accessibility, the shell's WM_GETICON) are left out.
bool CreateWindowExA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 12)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 CreateWindowExA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 CreateWindowExA needs the guest process");
    }
    const auto& arguments = call.arguments;
    const std::uint32_t ex_style = arguments[0];
    const std::uint32_t style = arguments[3];
    if ((ex_style & ~kWsExAppWindow) != 0 || (style & kWsPopup) == 0 || (style & kWsUnmodelled) != 0 ||
        arguments[8] != 0 || arguments[9] != 0 || arguments[4] == kCwUseDefault ||
        arguments[6] == kCwUseDefault)
    {
        return Fail(error, "user32 CreateWindowExA has no model of this window shape");
    }
    if (IsIntResource(arguments[1]))
    {
        return Fail(error, "user32 CreateWindowExA has no model of an atom class name");
    }
    std::string class_name;
    std::string read_error;
    if (!call.services->ReadGuestString(runtime::GuestAddress(arguments[1]), &class_name, &read_error))
    {
        return Fail(error, "user32 CreateWindowExA cannot read the class name: " + read_error);
    }
    GuestUser& user = process->user();
    const GuestWindowClass* window_class = user.FindClass(class_name, ClassInstance(*process, arguments[10]));
    if (window_class == nullptr)
    {
        return Fail(error, "user32 CreateWindowExA has no model of an unregistered class: " + class_name);
    }
    GuestWindow window;
    window.class_atom = window_class->atom;
    window.window_procedure = window_class->window_procedure;
    // Windows adds WS_CLIPSIBLINGS to every top-level window.
    window.style = (style & ~kWsVisible) | kWsClipSiblings;
    window.ex_style = ex_style;
    window.instance = arguments[10];
    window.x = static_cast<std::int32_t>(arguments[4]);
    window.y = static_cast<std::int32_t>(arguments[5]);
    window.width = static_cast<std::int32_t>(arguments[6]);
    window.height = static_cast<std::int32_t>(arguments[7]);
    window.extra.assign(window_class->window_extra, 0);
    if (arguments[2] != 0 &&
        !call.services->ReadGuestString(runtime::GuestAddress(arguments[2]), &window.title, &read_error))
    {
        return Fail(error, "user32 CreateWindowExA cannot read the window name: " + read_error);
    }
    const std::uint32_t handle = user.AddWindow(std::move(window));

    // CREATESTRUCTA: lpCreateParams, hInstance, hMenu, hwndParent, cy, cx, y,
    // x, style, lpszName, lpszClass, dwExStyle. The name and class point at
    // the caller's strings.
    const auto create_struct = [&arguments]() {
        std::vector<std::uint8_t> bytes;
        for (const std::size_t index : {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0})
        {
            PutU32(&bytes, arguments[index]);
        }
        return bytes;
    };
    std::uint32_t answer = 0;
    std::vector<std::uint8_t> data = create_struct();
    if (!SendToWindow(call, *user.LookupWindow(handle), kWmNcCreate, 0, 0, &data, &answer, error))
    {
        return false;
    }
    if (answer == 0)
    {
        return Fail(error, "user32 CreateWindowExA has no model of a failed WM_NCCREATE");
    }
    // WM_NCCALCSIZE(FALSE, RECT*): the window rectangle in, the client
    // rectangle out.
    {
        const GuestWindow& created = *user.LookupWindow(handle);
        data.clear();
        PutU32(&data, static_cast<std::uint32_t>(created.x));
        PutU32(&data, static_cast<std::uint32_t>(created.y));
        PutU32(&data, static_cast<std::uint32_t>(created.x + created.width));
        PutU32(&data, static_cast<std::uint32_t>(created.y + created.height));
    }
    if (!SendToWindow(call, *user.LookupWindow(handle), kWmNcCalcSize, 0, 0, &data, &answer, error))
    {
        return false;
    }
    {
        GuestWindow& created = *user.LookupWindow(handle);
        created.client_left = static_cast<std::int32_t>(ReadU32(&data[0])) - created.x;
        created.client_top = static_cast<std::int32_t>(ReadU32(&data[4])) - created.y;
        created.client_right = static_cast<std::int32_t>(ReadU32(&data[8])) - created.x;
        created.client_bottom = static_cast<std::int32_t>(ReadU32(&data[12])) - created.y;
    }
    data = create_struct();
    if (!SendToWindow(call, *user.LookupWindow(handle), kWmCreate, 0, 0, &data, &answer, error))
    {
        return false;
    }
    if (answer == 0xFFFFFFFFU)
    {
        return Fail(error, "user32 CreateWindowExA has no model of a failed WM_CREATE");
    }
    {
        const GuestWindow created = *user.LookupWindow(handle);
        if (!SendToWindow(call, created, kWmSize, 0,
                          PackPoint(created.client_right - created.client_left,
                                    created.client_bottom - created.client_top),
                          nullptr, &answer, error) ||
            !SendToWindow(call, created, kWmMove, 0,
                          PackPoint(created.x + created.client_left, created.y + created.client_top),
                          nullptr, &answer, error))
        {
            return false;
        }
    }
    if ((style & kWsVisible) != 0 && !ShowHiddenWindow(call, user, handle, error))
    {
        return false;
    }
    result->eax = handle;
    return Succeed(error);
}

// DefWindowProcA(hWnd, Msg, wParam, lParam) for the messages the window model
// sends, with the results Windows 11 gives a WS_POPUP window. WM_ACTIVATE
// gives the focus to an activated window, as Windows does; the IME and
// accessibility messages Windows sends from there are left out. Painting
// messages draw nothing yet.
bool DefWindowProcA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 DefWindowProcA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 DefWindowProcA needs the guest process");
    }
    GuestUser& user = process->user();
    const GuestWindow* window = user.LookupWindow(call.arguments[0]);
    if (window == nullptr)
    {
        return Fail(error, "user32 DefWindowProcA has no model of an unknown window");
    }
    const std::uint32_t message = call.arguments[1];
    const std::uint32_t w_param = call.arguments[2];
    switch (message)
    {
    case kWmNcCreate:
    case kWmNcActivate:
        result->eax = 1;
        return Succeed(error);
    case kWmEraseBackground:
    {
        // Filling with the class brush reports the background erased.
        const GuestWindowClass* window_class = user.FindClass(window->class_atom);
        result->eax = window_class != nullptr && window_class->background != 0 ? 1 : 0;
        return Succeed(error);
    }
    case kWmActivate:
        if ((w_param & 0xFFFFU) != kWaInactive && (w_param >> 16) == 0 &&
            !SetFocusTo(call, user, window->handle, error))
        {
            return false;
        }
        return Succeed(error);
    case kWmPaint:
        // BeginPaint and EndPaint: the update region is validated.
        user.LookupWindow(window->handle)->needs_paint = false;
        return Succeed(error);
    case kWmWindowPosChanged:
    {
        // With the size and position unchanged there is no WM_SIZE or WM_MOVE
        // to send.
        std::array<std::uint8_t, 28> position{};
        std::string read_error;
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[3]), position, &read_error))
        {
            return Fail(error, "user32 DefWindowProcA cannot read WINDOWPOS: " + read_error);
        }
        if ((ReadU32(&position[24]) & (kSwpNoSize | kSwpNoMove)) != (kSwpNoSize | kSwpNoMove))
        {
            return Fail(error, "user32 DefWindowProcA has no model of a moved or sized window");
        }
        return Succeed(error);
    }
    case kWmNcCalcSize:
    {
        // WM_NCCALCSIZE(FALSE, RECT*): the client area is the window
        // rectangle, one pixel in on each side with WS_BORDER, as measured.
        if (call.arguments[2] != 0 || (window->style & kWsBorder) == 0)
        {
            return Succeed(error);
        }
        std::array<std::uint8_t, 16> rect{};
        std::string memory_error;
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[3]), rect, &memory_error))
        {
            return Fail(error, "user32 DefWindowProcA cannot read the WM_NCCALCSIZE rectangle: " + memory_error);
        }
        std::vector<std::uint8_t> inset;
        PutU32(&inset, ReadU32(&rect[0]) + 1);
        PutU32(&inset, ReadU32(&rect[4]) + 1);
        PutU32(&inset, ReadU32(&rect[8]) - 1);
        PutU32(&inset, ReadU32(&rect[12]) - 1);
        if (!call.services->WriteGuestBytes(runtime::GuestAddress(call.arguments[3]), inset, &memory_error))
        {
            return Fail(error, "user32 DefWindowProcA cannot write the WM_NCCALCSIZE rectangle: " + memory_error);
        }
        return Succeed(error);
    }
    case kWmCreate:
    case kWmMove:
    case kWmSize:
    case kWmSetFocus:
    case kWmKillFocus:
    case kWmShowWindow:
    case kWmActivateApp:
    case kWmWindowPosChanging:
    case kWmNcPaint:
        return Succeed(error);
    default:
    {
        char text[80];
        std::snprintf(text, sizeof(text), "user32 DefWindowProcA has no model of message %04x", message);
        return Fail(error, text);
    }
    }
}

// UpdateWindow(hWnd): sends WM_PAINT when the window has an update region,
// as Windows 11 does once after creation; with none it sends nothing. Both
// return TRUE without touching the last error.
// GetAsyncKeyState(vKey), as measured on Windows 11: a key code outside
// 0..255 (with no masking of its high bits) is 0 with
// ERROR_INVALID_PARAMETER; a key code inside leaves the last error alone.
// A key the host holds reads 0x8000. The low bit, "pressed since the last
// query", is not modelled; Windows documents it as unreliable.
bool GetAsyncKeyState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 GetAsyncKeyState argument shape is invalid");
    }
    *result = {};
    if (call.arguments[0] > 0xFFU)
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return Succeed(error);
    }
    const HostPresentation* presentation = call.services->Presentation();
    if (presentation != nullptr && presentation->Input().virtual_keys.test(call.arguments[0]))
    {
        result->eax = 0x8000U;
    }
    return Succeed(error);
}

// The COLORREF a brush fills with, as FillRect finds it: a solid brush of
// the process, or a stock brush (the null brush fills nothing). False for a
// handle that is neither.
bool BrushColor(GuestProcess& process, std::uint32_t brush, std::optional<std::uint32_t>* color)
{
    if (const std::uint32_t* solid = process.gdi().FindBrush(brush); solid != nullptr)
    {
        *color = *solid;
        return true;
    }
    // WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH.
    constexpr std::array<std::uint32_t, 5> kStockColors = {0xFFFFFFU, 0xC0C0C0U, 0x808080U, 0x404040U, 0x000000U};
    for (std::uint32_t index = 0; index < kStockColors.size(); ++index)
    {
        if (brush == StockObjectHandle(index))
        {
            *color = kStockColors[index];
            return true;
        }
    }
    constexpr std::uint32_t kNullBrush = 5;
    if (brush == StockObjectHandle(kNullBrush))
    {
        color->reset();
        return true;
    }
    return false;
}

// FillRect(hDC, lprc, hbr), as measured on Windows 11 on a 16-bit DIB: the
// rectangle ordered and clipped to the DC's bitmap is filled with the brush's
// color narrowed channel by channel; the result is 1 and the last error
// stays. A handle that is no brush fills nothing and still returns 1; a DC
// that is none is 0 with ERROR_INVALID_HANDLE. Not modelled, and stopping:
// a null rectangle (a fault on Windows), a system color index as the brush,
// a palette COLORREF, and a bitmap other than 16-bit.
bool FillRect(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 3)
    {
        return Fail(error, result == nullptr ? "user32 result is null" : "user32 FillRect argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 FillRect needs the guest process");
    }
    const GuestDc* dc = process->gdi().FindDc(call.arguments[0]);
    if (dc == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return Succeed(error);
    }
    if (call.arguments[1] == 0)
    {
        return Fail(error, "user32 FillRect with a null rectangle faults on Windows");
    }
    const std::uint32_t brush = call.arguments[2];
    if (brush >= 1 && brush <= 31)
    {
        return Fail(error, "user32 FillRect with a system color brush is not modelled");
    }
    std::optional<std::uint32_t> color;
    if (!BrushColor(*process, brush, &color) || !color.has_value())
    {
        result->eax = 1;
        return Succeed(error);
    }
    if ((*color >> 24) != 0)
    {
        return Fail(error, "user32 FillRect with a palette color is not modelled");
    }
    const GuestBitmap* bitmap = process->gdi().FindBitmap(dc->bitmap);
    if (bitmap == nullptr || bitmap->bits_per_pixel != 16)
    {
        return Fail(error, "user32 FillRect has no model of this DC's bitmap");
    }
    std::array<std::uint8_t, 16> rect_bytes{};
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[1]), rect_bytes, &read_error))
    {
        return Fail(error, "user32 FillRect cannot read the RECT: " + read_error);
    }
    GdiRect rect;
    std::memcpy(&rect, rect_bytes.data(), sizeof(rect_bytes));
    const GdiRect area = FillArea(rect, bitmap->width, bitmap->height);
    const bool has_masks = bitmap->masks[0] != 0 || bitmap->masks[1] != 0 || bitmap->masks[2] != 0;
    const GdiPixelLayout layout = has_masks ? GdiPixelLayout{16, bitmap->masks} : kGdiRgb555;
    const std::uint32_t pixel = ConvertGdiPixel(*color, kGdiColorref, layout);
    if (!area.empty())
    {
        std::vector<std::uint8_t> row(static_cast<std::size_t>(area.right - area.left) * 2);
        for (std::size_t offset = 0; offset < row.size(); offset += 2)
        {
            WriteGdiPixel(std::span<std::uint8_t>(row).subspan(offset), 16, pixel);
        }
        for (std::int32_t y = area.top; y < area.bottom; ++y)
        {
            const std::uint32_t line =
                bitmap->top_down ? static_cast<std::uint32_t>(y) : bitmap->height - 1 - static_cast<std::uint32_t>(y);
            const std::uint32_t address = bitmap->bits + line * bitmap->pitch + static_cast<std::uint32_t>(area.left) * 2;
            std::string write_error;
            if (!call.services->WriteGuestBytes(runtime::GuestAddress(address), row, &write_error))
            {
                return Fail(error, "user32 FillRect cannot write the bitmap: " + write_error);
            }
        }
    }
    result->eax = 1;
    return Succeed(error);
}

// Division rounding down, as DrawTextA halves the room around its text.
std::int32_t HalfDown(std::int32_t value)
{
    return value >= 0 ? value / 2 : -((-value + 1) / 2);
}

// Draws one line of text into a DC's 16-bit bitmap for DrawTextA: the cell
// placed by the format, painted in the background color when OPAQUE, then
// the glyphs' set pixels in the text color, all clipped.
bool DrawTextPixels(const ImportCall& call,
                    GuestProcess& process,
                    std::uint32_t dc_handle,
                    std::string_view text,
                    const GdiRect& rect,
                    std::uint32_t format,
                    std::string* error)
{
    constexpr std::uint32_t kDtCenter = 0x01;
    constexpr std::uint32_t kDtRight = 0x02;
    constexpr std::uint32_t kDtVCenter = 0x04;
    constexpr std::uint32_t kDtBottom = 0x08;
    constexpr std::uint32_t kDtNoClip = 0x100;
    constexpr std::uint32_t kOpaque = 2;
    const GuestDc* dc = process.gdi().FindDc(dc_handle);
    const GuestBitmap* bitmap = dc == nullptr ? nullptr : process.gdi().FindBitmap(dc->bitmap);
    if (bitmap == nullptr || bitmap->bits_per_pixel != 16)
    {
        return Fail(error, "user32 DrawTextA has no model of this DC's bitmap");
    }
    const bool opaque = dc->background_mode == kOpaque;
    if ((dc->text_color >> 24) != 0 || (opaque && (dc->background_color >> 24) != 0))
    {
        return Fail(error, "user32 DrawTextA with a palette color is not modelled");
    }
    const auto text_width = static_cast<std::int32_t>(text.size()) * kGuestFontCharWidth;
    std::int32_t x = rect.left;
    if ((format & kDtCenter) != 0)
    {
        x = rect.left + HalfDown(rect.right - rect.left - text_width);
    }
    else if ((format & kDtRight) != 0)
    {
        x = rect.right - text_width;
    }
    std::int32_t y = rect.top;
    if ((format & kDtVCenter) != 0)
    {
        y = rect.top + HalfDown(rect.bottom - rect.top - kGuestFontHeight);
    }
    else if ((format & kDtBottom) != 0)
    {
        y = rect.bottom - kGuestFontHeight;
    }
    GdiRect clip = FillArea((format & kDtNoClip) != 0 ? GdiRect{0, 0, static_cast<std::int32_t>(bitmap->width),
                                                                  static_cast<std::int32_t>(bitmap->height)}
                                                         : rect,
                            bitmap->width, bitmap->height);
    clip.left = std::max(clip.left, x);
    clip.top = std::max(clip.top, y);
    clip.right = std::min(clip.right, x + text_width);
    clip.bottom = std::min(clip.bottom, y + kGuestFontHeight);
    if (clip.empty())
    {
        return true;
    }
    const bool has_masks = bitmap->masks[0] != 0 || bitmap->masks[1] != 0 || bitmap->masks[2] != 0;
    const GdiPixelLayout layout = has_masks ? GdiPixelLayout{16, bitmap->masks} : kGdiRgb555;
    const std::uint32_t text_pixel = ConvertGdiPixel(dc->text_color, kGdiColorref, layout);
    const std::uint32_t background_pixel = ConvertGdiPixel(dc->background_color, kGdiColorref, layout);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(clip.right - clip.left) * 2);
    for (std::int32_t py = clip.top; py < clip.bottom; ++py)
    {
        const std::uint32_t line =
            bitmap->top_down ? static_cast<std::uint32_t>(py) : bitmap->height - 1 - static_cast<std::uint32_t>(py);
        const std::uint32_t address = bitmap->bits + line * bitmap->pitch + static_cast<std::uint32_t>(clip.left) * 2;
        std::string access_error;
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(address), row, &access_error))
        {
            return Fail(error, "user32 DrawTextA cannot read the bitmap: " + access_error);
        }
        for (std::int32_t px = clip.left; px < clip.right; ++px)
        {
            const std::int32_t column = px - x;
            const auto* glyph = GuestFontGlyph(static_cast<unsigned char>(text[static_cast<std::size_t>(column / kGuestFontCharWidth)]));
            const bool set = ((*glyph)[static_cast<std::size_t>(py - y)] & (0x80U >> (column % kGuestFontCharWidth))) != 0;
            const auto span = std::span<std::uint8_t>(row).subspan(static_cast<std::size_t>(px - clip.left) * 2);
            if (set)
            {
                WriteGdiPixel(span, 16, text_pixel);
            }
            else if (opaque)
            {
                WriteGdiPixel(span, 16, background_pixel);
            }
        }
        if (!call.services->WriteGuestBytes(runtime::GuestAddress(address), row, &access_error))
        {
            return Fail(error, "user32 DrawTextA cannot write the bitmap: " + access_error);
        }
    }
    return true;
}

// DrawTextA(hDC, lpchText, cchText, lprc, format) for one line in a DC's
// default font, the Korean "System" bitmap font 16 pixels high, as measured
// on Windows 11: the result is the offset from the rectangle's top to the
// bottom of the text (16 at the top, (height - 16) / 2 + 16 centred, the
// height at the bottom); drawing text sets the last error to 0. A count of
// 0 or a null text is 0 with the last error left; an empty string still
// answers the offset, leaving the last error; a handle that is no DC is 0
// with ERROR_INVALID_PARAMETER.
//
// The text is drawn with GNU Unifont's 8x16 glyphs standing in for the
// System font (see guest_font.h), in the DC's text color, placed as measured:
// left, centred (halves rounded down), or right, and top, centred, or at the
// bottom of the rectangle, clipped to it unless DT_NOCLIP, and to the bitmap.
// In OPAQUE mode the text cell is first painted in the background color.
// Formats other than a single line (DT_CALCRECT, word breaks, prefixes, and
// the rest), bytes outside printable ASCII (CP949's double-byte characters
// are not modelled), palette colors, and bitmaps other than 16-bit stop.
bool DrawTextA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kDtVCenter = 0x04;
    constexpr std::uint32_t kDtBottom = 0x08;
    constexpr std::uint32_t kDtSingleLine = 0x20;
    constexpr std::uint32_t kDtNoClip = 0x100;
    constexpr std::uint32_t kModelled = 0x03 | kDtVCenter | kDtBottom | kDtSingleLine | kDtNoClip;
    constexpr std::int32_t kFontHeight = 16;
    if (result == nullptr || call.arguments.size() != 5)
    {
        return Fail(error, result == nullptr ? "user32 result is null" : "user32 DrawTextA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 DrawTextA needs the guest process");
    }
    if (process->gdi().FindDc(call.arguments[0]) == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return Succeed(error);
    }
    const std::uint32_t format = call.arguments[4];
    if ((format & ~kModelled) != 0 || (format & kDtSingleLine) == 0 || (format & 0x03) == 0x03 ||
        (format & (kDtVCenter | kDtBottom)) == (kDtVCenter | kDtBottom))
    {
        return Fail(error, "user32 DrawTextA format " + std::to_string(format) + " is not modelled");
    }
    const auto count = static_cast<std::int32_t>(call.arguments[2]);
    if (call.arguments[1] == 0 || count == 0)
    {
        return Succeed(error);
    }
    if (call.arguments[3] == 0)
    {
        return Fail(error, "user32 DrawTextA with a null rectangle is not modelled");
    }
    std::string text;
    std::string read_error;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]), &text, &read_error) &&
        !read_error.empty())
    {
        return Fail(error, "user32 DrawTextA cannot read the text: " + read_error);
    }
    const std::size_t length = count < 0 ? text.size() : std::min<std::size_t>(text.size(), static_cast<std::size_t>(count));
    for (std::size_t index = 0; index < length; ++index)
    {
        if (GuestFontGlyph(static_cast<unsigned char>(text[index])) == nullptr)
        {
            return Fail(error, "user32 DrawTextA of a character outside printable ASCII is not modelled");
        }
    }
    std::array<std::uint8_t, 16> rect_bytes{};
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[3]), rect_bytes, &read_error))
    {
        return Fail(error, "user32 DrawTextA cannot read the RECT: " + read_error);
    }
    GdiRect rect;
    std::memcpy(&rect, rect_bytes.data(), sizeof(rect_bytes));
    const std::int32_t height = rect.bottom - rect.top;
    std::int32_t offset = kFontHeight;
    if ((format & kDtVCenter) != 0)
    {
        offset = (height - kFontHeight) / 2 + kFontHeight;
    }
    else if ((format & kDtBottom) != 0)
    {
        offset = height;
    }
    if (length != 0 &&
        !DrawTextPixels(call, *process, call.arguments[0], std::string_view(text).substr(0, length), rect, format, error))
    {
        return false;
    }
    result->eax = static_cast<std::uint32_t>(offset);
    if (length != 0)
    {
        call.services->SetLastError(kWin32ErrorSuccess);
    }
    return Succeed(error);
}

// GetCursorPos(lpPoint): the cursor's screen position. Measured on Windows
// 11: a null pointer is FALSE with ERROR_NOACCESS, and success leaves the
// last error alone.
bool GetCursorPos(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 GetCursorPos argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 GetCursorPos needs the guest process");
    }
    if (call.arguments[0] == 0)
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return Succeed(error);
    }
    // The host's pointer over the guest window, from that window's client
    // origin on the screen; the model's resting position until then.
    std::int32_t x = process->user().cursor_x();
    std::int32_t y = process->user().cursor_y();
    const HostPresentation* presentation = call.services->Presentation();
    if (presentation != nullptr)
    {
        const HostInputState& input = presentation->Input();
        const GuestWindow* window =
            input.cursor_window == 0 ? nullptr : process->user().LookupWindow(input.cursor_window);
        if (window != nullptr)
        {
            x = window->x + window->client_left + input.cursor_x;
            y = window->y + window->client_top + input.cursor_y;
        }
    }
    const std::array<std::uint32_t, 2> point = {static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
    std::array<std::uint8_t, 8> bytes{};
    std::memcpy(bytes.data(), point.data(), bytes.size());
    std::string write_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &write_error))
    {
        return Fail(error, "user32 GetCursorPos cannot write the POINT: " + write_error);
    }
    result->eax = 1;
    return Succeed(error);
}

// ScreenToClient(hWnd, lpPoint): the point less the window's client origin
// on the screen. Measured on Windows 11: an unknown window or a null point is
// FALSE with ERROR_INVALID_WINDOW_HANDLE, leaving the point alone; success
// leaves the last error alone.
bool ScreenToClient(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 2)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 ScreenToClient argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 ScreenToClient needs the guest process");
    }
    const GuestWindow* window = process->user().LookupWindow(call.arguments[0]);
    if (window == nullptr || call.arguments[1] == 0)
    {
        call.services->SetLastError(kWin32ErrorInvalidWindowHandle);
        return Succeed(error);
    }
    std::array<std::uint8_t, 8> bytes{};
    std::string access_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[1]), bytes, &access_error))
    {
        return Fail(error, "user32 ScreenToClient cannot read the POINT: " + access_error);
    }
    std::array<std::int32_t, 2> point{};
    std::memcpy(point.data(), bytes.data(), bytes.size());
    point[0] -= window->x + window->client_left;
    point[1] -= window->y + window->client_top;
    std::memcpy(bytes.data(), point.data(), bytes.size());
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(call.arguments[1]), bytes, &access_error))
    {
        return Fail(error, "user32 ScreenToClient cannot write the POINT: " + access_error);
    }
    result->eax = 1;
    return Succeed(error);
}

constexpr std::uint32_t kWmTimer = 0x0113;
constexpr std::uint32_t kPmRemove = 0x0001;

// PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg) for the
// whole thread, as measured on Windows 11. The queue is looked at in
// Windows' order: posted messages (none are posted yet), WM_PAINT for a
// window with an update region (left in place), then WM_TIMER for a thread
// timer whose interval has passed; taking a WM_TIMER restarts the interval
// from now, so late timers coalesce. The MSG's time is now and its point the
// cursor. With nothing to give, FALSE leaves the MSG and the last error
// alone. Window or range filters and other flags are not modelled.
bool PeekMessageA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 5)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 PeekMessageA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 PeekMessageA needs the guest process");
    }
    if (call.arguments[0] == 0 || call.arguments[1] != 0 || call.arguments[2] != 0 || call.arguments[3] != 0 ||
        (call.arguments[4] & ~kPmRemove) != 0)
    {
        return Fail(error, "user32 PeekMessageA has no model of this filter or these flags");
    }
    GuestClockReading clock;
    if (!call.services->ReadClock(&clock))
    {
        return Fail(error, "user32 PeekMessageA needs the host clock");
    }
    const bool remove = (call.arguments[4] & kPmRemove) != 0;
    std::array<std::uint32_t, 7> message{};
    bool found = false;
    for (const auto& [handle, window] : process->user().windows())
    {
        if (window.needs_paint)
        {
            message = {window.handle, kWmPaint, 0, 0, 0, 0, 0};
            found = true;
            break;
        }
    }
    if (!found)
    {
        if (GuestTimer* timer = process->DueTimer(clock.tick_ms))
        {
            message = {0, kWmTimer, timer->id, timer->procedure, 0, 0, 0};
            if (remove)
            {
                timer->base_tick = clock.tick_ms;
            }
            found = true;
        }
    }
    if (!found)
    {
        return Succeed(error);
    }
    message[4] = clock.tick_ms;
    message[5] = static_cast<std::uint32_t>(process->user().cursor_x());
    message[6] = static_cast<std::uint32_t>(process->user().cursor_y());
    std::array<std::uint8_t, 28> bytes{};
    std::memcpy(bytes.data(), message.data(), bytes.size());
    std::string write_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &write_error))
    {
        return Fail(error, "user32 PeekMessageA cannot write the MSG: " + write_error);
    }
    result->eax = 1;
    return Succeed(error);
}

// Reads a MSG {hwnd, message, wParam, lParam, time, pt}.
bool ReadMessage(const ImportCall& call, std::array<std::uint32_t, 7>* message, std::string* error)
{
    std::array<std::uint8_t, 28> bytes{};
    std::string read_error;
    if (call.arguments[0] == 0 ||
        !call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &read_error))
    {
        return Fail(error, "user32 " + call.gate.name + " cannot read the MSG" +
                               (read_error.empty() ? std::string() : ": " + read_error));
    }
    std::memcpy(message->data(), bytes.data(), bytes.size());
    return true;
}

// TranslateMessage(lpMsg): FALSE, the last error untouched, for anything but
// a key message, which would post characters and is not modelled.
bool TranslateMessage(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 TranslateMessage argument shape is invalid");
    }
    *result = {};
    std::array<std::uint32_t, 7> message{};
    if (!ReadMessage(call, &message, error))
    {
        return false;
    }
    if (message[1] >= 0x0100U && message[1] <= 0x0109U)
    {
        return Fail(error, "user32 TranslateMessage has no model of key messages");
    }
    return Succeed(error);
}

// DispatchMessageA(lpMsg), as measured on Windows 11: a WM_TIMER with the
// procedure of a live timer calls it as (hwnd, WM_TIMER, id, now) and returns
// what it returns; a WM_TIMER with any other procedure, or a message for no
// window, is 0 and calls nothing. A message for a window goes to its window
// procedure.
bool DispatchMessageA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 DispatchMessageA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 DispatchMessageA needs the guest process");
    }
    std::array<std::uint32_t, 7> message{};
    if (!ReadMessage(call, &message, error))
    {
        return false;
    }
    if (message[1] == kWmTimer && message[3] != 0)
    {
        if (process->FindTimer(message[2], message[3]) == nullptr)
        {
            return Succeed(error);
        }
        GuestClockReading clock;
        if (!call.services->ReadClock(&clock))
        {
            return Fail(error, "user32 DispatchMessageA needs the host clock");
        }
        GuestCall guest_call;
        guest_call.function = message[3];
        guest_call.arguments = {message[0], kWmTimer, message[2], clock.tick_ms};
        std::string call_error;
        if (!call.services->CallGuest(&guest_call, &result->eax, &call_error))
        {
            return Fail(error, "user32 DispatchMessageA cannot call the timer procedure: " + call_error);
        }
        return Succeed(error);
    }
    if (message[0] == 0)
    {
        return Succeed(error);
    }
    const GuestWindow* window = process->user().LookupWindow(message[0]);
    if (window == nullptr)
    {
        return Fail(error, "user32 DispatchMessageA has no model of a message for an unknown window");
    }
    const GuestWindow target = *window;
    return SendToWindow(call, target, message[1], message[2], message[3], nullptr, &result->eax, error) &&
           Succeed(error);
}

// GetWindowLong indices (winuser.h).
constexpr std::int32_t kGwlWndProc = -4;
constexpr std::int32_t kGwlHInstance = -6;
constexpr std::int32_t kGwlHwndParent = -8;
constexpr std::int32_t kGwlId = -12;
constexpr std::int32_t kGwlStyle = -16;
constexpr std::int32_t kGwlExStyle = -20;
constexpr std::int32_t kGwlUserData = -21;

// GetWindowLongA(hWnd, nIndex), as measured on Windows 11 for an ANSI
// window: the negative GWL_ indices name the window's own fields, a
// non-negative one a DWORD of its cbWndExtra bytes. Success leaves the last
// error alone; an unknown index is 0 with ERROR_INVALID_INDEX, an unknown
// window 0 with ERROR_INVALID_WINDOW_HANDLE.
bool GetWindowLongA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 2)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 GetWindowLongA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 GetWindowLongA needs the guest process");
    }
    const GuestWindow* window = process->user().LookupWindow(call.arguments[0]);
    if (window == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidWindowHandle);
        return Succeed(error);
    }
    const auto index = static_cast<std::int32_t>(call.arguments[1]);
    switch (index)
    {
    case kGwlWndProc:
        result->eax = window->window_procedure;
        return Succeed(error);
    case kGwlHInstance:
        result->eax = window->instance;
        return Succeed(error);
    case kGwlHwndParent:
        result->eax = window->parent;
        return Succeed(error);
    case kGwlId:
        result->eax = window->menu;
        return Succeed(error);
    case kGwlStyle:
        result->eax = window->style;
        return Succeed(error);
    case kGwlExStyle:
        result->eax = window->ex_style;
        return Succeed(error);
    case kGwlUserData:
        result->eax = window->user_data;
        return Succeed(error);
    default:
        break;
    }
    if (index < 0 || static_cast<std::size_t>(index) + 4 > window->extra.size())
    {
        call.services->SetLastError(kWin32ErrorInvalidIndex);
        return Succeed(error);
    }
    const auto* bytes = window->extra.data() + index;
    result->eax = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
                  (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
    return Succeed(error);
}

bool UpdateWindow(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 UpdateWindow argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 UpdateWindow needs the guest process");
    }
    const GuestWindow* window = process->user().LookupWindow(call.arguments[0]);
    if (window == nullptr)
    {
        return Fail(error, "user32 UpdateWindow has no model of an unknown window");
    }
    if (window->needs_paint)
    {
        const GuestWindow painted = *window;
        std::uint32_t answer = 0;
        if (!SendToWindow(call, painted, kWmPaint, 0, 0, nullptr, &answer, error))
        {
            return false;
        }
    }
    result->eax = 1;
    return Succeed(error);
}

// ShowCursor(bShow): the new display counter. Measured on Windows 11: from 0,
// three FALSE calls give -1, -2, -3 and a TRUE call -2.
bool ShowCursor(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 ShowCursor argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "user32 ShowCursor needs the guest process");
    }
    result->eax = static_cast<std::uint32_t>(process->user().ShowCursor(call.arguments[0] != 0));
    return Succeed(error);
}

// SetRect(lprc, xLeft, yTop, xRight, yBottom): writes the RECT as given.
// Measured on Windows 11: NULL gives FALSE, reversed coordinates are stored
// unchanged, and the last error never changes.
bool SetRect(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 5)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 SetRect argument shape is invalid");
    }
    *result = {};
    if (call.arguments[0] == 0)
    {
        return Succeed(error);
    }
    std::array<std::uint8_t, 16> bytes{};
    for (std::size_t index = 0; index < 4; ++index)
    {
        const std::uint32_t value = call.arguments[1 + index];
        for (std::size_t shift = 0; shift < 4; ++shift)
        {
            bytes[index * 4 + shift] = static_cast<std::uint8_t>(value >> (shift * 8));
        }
    }
    std::string write_error;
    if (call.services == nullptr ||
        !call.services->WriteGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &write_error))
    {
        return Fail(error, "user32 SetRect cannot write the RECT: " + write_error);
    }
    result->eax = 1;
    return Succeed(error);
}

// wsprintfA(lpOut, lpFmt, ...), cdecl: the shared core formats from the
// variadic arguments on the guest stack past the two declared ones, as
// measured on Windows 11 (design 419). At most 1024 characters are written,
// then the terminator; the count written is returned and the last error is
// left alone. A NULL lpOut or lpFmt would fault on Windows and stops.
bool WsprintfA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 2)
    {
        return Fail(error, result == nullptr ? "user32 result is null"
                                             : "user32 wsprintfA argument shape is invalid");
    }
    *result = {};
    if (call.services == nullptr || call.arguments_address == 0)
    {
        return Fail(error, "user32 wsprintfA needs the guest's stack arguments");
    }
    if (call.arguments[0] == 0 || call.arguments[1] == 0)
    {
        return Fail(error, "user32 wsprintfA with a NULL buffer or format is not modelled");
    }
    std::string format;
    std::string read_error;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]), &format, &read_error))
    {
        return Fail(error, "user32 wsprintfA cannot read the format: " + read_error);
    }
    std::uint32_t next = call.arguments_address + 2 * sizeof(std::uint32_t);
    const WsprintfWordReader next_word = [&](std::uint32_t* word) {
        std::array<std::uint8_t, 4> bytes{};
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(next), bytes, &read_error))
        {
            return false;
        }
        next += 4;
        *word = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
                (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
        return true;
    };
    const WsprintfStringReader read_string = [&](std::uint32_t address, std::string* text) {
        return call.services->ReadGuestString(runtime::GuestAddress(address), text, &read_error);
    };
    std::string output;
    std::string format_error;
    if (!FormatWsprintf(format, next_word, read_string, &output, &format_error))
    {
        return Fail(error, "user32 " + format_error + (read_error.empty() ? "" : ": " + read_error));
    }
    const auto count = static_cast<std::uint32_t>(output.size());
    output.push_back('\0');
    const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(output.data()), output.size());
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &read_error))
    {
        return Fail(error, "user32 wsprintfA cannot write the output: " + read_error);
    }
    result->eax = count;
    return Succeed(error);
}

// Exports the protection resolves without calling them yet, most while
// rebuilding the original program's import table (winuser.h signatures).
constexpr ResolveOnlyExport kUser32ResolveOnly[] = {
    {"CreateCursor", 7}, {"DestroyCursor", 1}, {"SetCursor", 1}, {"KillTimer", 2},
    {"ExitWindowsEx", 2}, {"PostQuitMessage", 1}, {"DestroyWindow", 1},
    {"ClientToScreen", 2}, {"DrawMenuBar", 1}, {"GetClientRect", 2}, {"RedrawWindow", 4},
    {"ReleaseDC", 2},
    {"GetDesktopWindow", 0},
    // EZ2DJ 1st's imports (Task 405).
    {"SendMessageA", 4},
};

GuestExportDescriptor MakeExport(std::string name,
                                 std::uint32_t argument_count,
                                 ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

}  // namespace

std::uint32_t MessageBoxDefaultButton(std::uint32_t type)
{
    // Button sets by MB_TYPEMASK value, in the order the box shows them.
    static constexpr std::array<std::array<std::uint32_t, 3>, 7> kButtons = {{
        {kIdOk, 0, 0},                      // MB_OK
        {kIdOk, kIdCancel, 0},              // MB_OKCANCEL
        {kIdAbort, kIdRetry, kIdIgnore},    // MB_ABORTRETRYIGNORE
        {kIdYes, kIdNo, kIdCancel},         // MB_YESNOCANCEL
        {kIdYes, kIdNo, 0},                 // MB_YESNO
        {kIdRetry, kIdCancel, 0},           // MB_RETRYCANCEL
        {kIdCancel, kIdTryAgain, kIdContinue},  // MB_CANCELTRYCONTINUE
    }};
    const std::uint32_t set = type & 0xFU;
    const auto& buttons = kButtons[set < kButtons.size() ? set : 0];
    // MB_DEFBUTTON1..3 select a position; an empty or unknown position falls
    // back to the first button.
    const std::uint32_t position = (type & 0xF00U) >> 8;
    return position < buttons.size() && buttons[position] != 0 ? buttons[position]
                                                                : buttons[0];
}

GuestModuleDescriptor MakeUser32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "user32.dll";
    descriptor.aliases = {"user32"};
    descriptor.exports.push_back(MakeExport("GetActiveWindow", 0, &GetActiveWindow));
    descriptor.exports.push_back(MakeExport("GetForegroundWindow", 0, &GetForegroundWindow));
    descriptor.exports.push_back(MakeExport("MessageBoxA", 4, &MessageBoxA));
    descriptor.exports.push_back(MakeExport("SetTimer", 4, &SetTimer));
    descriptor.exports.push_back(MakeExport("LoadIconA", 2, &LoadIconA));
    descriptor.exports.push_back(MakeExport("LoadCursorA", 2, &LoadCursorA));
    descriptor.exports.push_back(MakeExport("RegisterClassA", 1, &RegisterClassA));
    descriptor.exports.push_back(MakeExport("CreateWindowExA", 12, &CreateWindowExA));
    descriptor.exports.push_back(MakeExport("DefWindowProcA", 4, &DefWindowProcA));
    descriptor.exports.push_back(MakeExport("UpdateWindow", 1, &UpdateWindow));
    descriptor.exports.push_back(MakeExport("ShowCursor", 1, &ShowCursor));
    descriptor.exports.push_back(MakeExport("SetRect", 5, &SetRect));
    descriptor.exports.push_back(MakeExport("GetWindowLongA", 2, &GetWindowLongA));
    descriptor.exports.push_back(MakeExport("GetAsyncKeyState", 1, &GetAsyncKeyState));
    descriptor.exports.push_back(MakeExport("GetCursorPos", 1, &GetCursorPos));
    descriptor.exports.push_back(MakeExport("ScreenToClient", 2, &ScreenToClient));
    descriptor.exports.push_back(MakeExport("PeekMessageA", 5, &PeekMessageA));
    descriptor.exports.push_back(MakeExport("TranslateMessage", 1, &TranslateMessage));
    descriptor.exports.push_back(MakeExport("DispatchMessageA", 1, &DispatchMessageA));
    descriptor.exports.push_back(MakeExport("FillRect", 3, &FillRect));
    descriptor.exports.push_back(MakeExport("LoadImageA", 6, &LoadImageA));
    descriptor.exports.push_back(MakeExport("DrawTextA", 5, &DrawTextA));
    descriptor.exports.push_back(MakeExport("ShowWindow", 2, &ShowWindow));
    descriptor.exports.push_back(MakeExport("EnumDisplaySettingsA", 3, &EnumDisplaySettingsA));
    descriptor.exports.push_back(MakeExport("ChangeDisplaySettingsExA", 5, &ChangeDisplaySettingsExA));
    GuestExportDescriptor wsprintf = MakeExport("wsprintfA", 2, &WsprintfA);
    wsprintf.calling_convention = CallingConvention::kCdecl;
    descriptor.exports.push_back(std::move(wsprintf));
    AddResolveOnlyExports(&descriptor, kUser32ResolveOnly);
    return descriptor;
}

}  // namespace re2dj::hle::modules
