#include "re2dj/hle/modules/user32_module.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/win32_errors.h"
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
    result->eax = process->SetThreadTimer(call.arguments[1], call.arguments[2], call.arguments[3]);
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
// Styles whose geometry or ownership the model does not have: WS_CHILD,
// WS_MINIMIZE, WS_MAXIMIZE, WS_CAPTION (WS_BORDER | WS_DLGFRAME), and
// WS_THICKFRAME.
constexpr std::uint32_t kWsUnmodelled = 0x40000000U | 0x20000000U | 0x01000000U | 0x00C00000U | 0x00040000U;
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

// CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y,
// nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) for a top-level
// WS_POPUP window without a menu, the shape 4th creates. The messages and
// their arguments follow what Windows 11 sends such a window (WOW64):
// WM_NCCREATE, WM_NCCALCSIZE, WM_CREATE, WM_SIZE, WM_MOVE, and for
// WS_VISIBLE the show and activation sequence. Messages Windows adds from
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
    if ((style & kWsVisible) != 0)
    {
        // ShowWindow(SW_SHOW) of a new top-level window: shown, brought to the
        // front, and activated with the keyboard focus. Its whole client area
        // is left to paint.
        user.LookupWindow(handle)->style |= kWsVisible;
        user.LookupWindow(handle)->needs_paint = true;
        const GuestWindow shown = *user.LookupWindow(handle);
        if (!SendToWindow(call, shown, kWmShowWindow, 1, 0, nullptr, &answer, error))
        {
            return false;
        }
        data = WindowPos(shown, false, kShowChangingFlags);
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
        if (!SendToWindow(call, shown, kWmWindowPosChanged, 0, 0, &data, &answer, error))
        {
            return false;
        }
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
    case kWmCreate:
    case kWmMove:
    case kWmSize:
    case kWmSetFocus:
    case kWmKillFocus:
    case kWmShowWindow:
    case kWmActivateApp:
    case kWmWindowPosChanging:
    case kWmNcCalcSize:
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

// Exports the protection resolves without calling them yet, most while
// rebuilding the original program's import table (winuser.h signatures;
// wsprintfA is variadic cdecl).
constexpr ResolveOnlyExport kUser32ResolveOnly[] = {
    {"CreateCursor", 7}, {"DestroyCursor", 1}, {"SetCursor", 1}, {"KillTimer", 2},
    {"ExitWindowsEx", 2}, {"PostQuitMessage", 1}, {"DestroyWindow", 1},
    {"DispatchMessageA", 1}, {"TranslateMessage", 1}, {"PeekMessageA", 5}, {"DrawTextA", 5},
    {"ClientToScreen", 2}, {"FillRect", 3}, {"DrawMenuBar", 1}, {"GetClientRect", 2}, {"RedrawWindow", 4},
    {"ReleaseDC", 2}, {"GetAsyncKeyState", 1},
    {"wsprintfA", 0, 0, CallingConvention::kCdecl},
    {"GetDesktopWindow", 0}, {"ScreenToClient", 2}, {"GetCursorPos", 1},
    {"ChangeDisplaySettingsExA", 5}, {"EnumDisplaySettingsA", 3},
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
    AddResolveOnlyExports(&descriptor, kUser32ResolveOnly);
    return descriptor;
}

}  // namespace re2dj::hle::modules
