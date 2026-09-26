#ifndef RE2DJ_HLE_GUEST_USER_H_
#define RE2DJ_HLE_GUEST_USER_H_

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace re2dj::hle
{

// A registered window class (WNDCLASSA with its name resolved).
struct GuestWindowClass
{
    std::string name;
    std::uint16_t atom = 0;
    std::uint32_t style = 0;
    std::uint32_t window_procedure = 0;
    std::uint32_t class_extra = 0;
    std::uint32_t window_extra = 0;
    std::uint32_t instance = 0;
    std::uint32_t icon = 0;
    std::uint32_t cursor = 0;
    std::uint32_t background = 0;
    std::uint32_t menu_name = 0;
};

// A window created by CreateWindowExA.
struct GuestWindow
{
    std::uint32_t handle = 0;
    std::uint16_t class_atom = 0;
    std::uint32_t window_procedure = 0;
    std::uint32_t style = 0;
    std::uint32_t ex_style = 0;
    std::uint32_t instance = 0;
    std::uint32_t parent = 0;
    std::uint32_t menu = 0;
    // The window rectangle in screen coordinates and the client rectangle in
    // window coordinates, as WM_NCCALCSIZE left it.
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t client_left = 0;
    std::int32_t client_top = 0;
    std::int32_t client_right = 0;
    std::int32_t client_bottom = 0;
    std::string title;
    // cbWndExtra bytes, zeroed at creation.
    std::vector<std::uint8_t> extra;
    // GWLP_USERDATA, 0 until the guest sets it.
    std::uint32_t user_data = 0;
    // The window's device context, which WM_ERASEBKGND hands over.
    std::uint32_t device_context = 0;
    // True while the window has an update region: from being shown until a
    // WM_PAINT is answered through BeginPaint or DefWindowProc.
    bool needs_paint = false;
};

// The guest's USER objects: shared system icons and cursors, window classes,
// and windows with the thread's active and focus windows. USER handles are
// their own space, apart from kernel handles.
class GuestUser
{
public:
    // The first atom RegisterClass hands out, as on Windows (MAXINTATOM).
    static constexpr std::uint16_t kFirstClassAtom = 0xC000U;

    // The shared handle of a system icon (IDI_*) or cursor (IDC_*), the same
    // on every call, or 0 for an ID the system does not have.
    std::uint32_t SystemIcon(std::uint32_t id);
    std::uint32_t SystemCursor(std::uint32_t id);
    bool IsIcon(std::uint32_t handle) const;
    bool IsCursor(std::uint32_t handle) const;

    // RegisterClassA: the new class's atom, or 0 when a class of that name is
    // already registered for the instance (ERROR_CLASS_ALREADY_EXISTS).
    std::uint16_t AddClass(GuestWindowClass window_class);
    // A class by name (case-insensitive) or by atom, for the instance.
    const GuestWindowClass* FindClass(std::string_view name, std::uint32_t instance) const;
    const GuestWindowClass* FindClass(std::uint16_t atom) const;

    // Adds a window and returns its new handle; the handle field is ignored.
    std::uint32_t AddWindow(GuestWindow window);
    GuestWindow* LookupWindow(std::uint32_t handle);

    // ShowCursor: moves the display counter up or down and returns it. The
    // cursor shows while the counter is 0 or more; with a mouse installed it
    // starts at 0.
    std::int32_t ShowCursor(bool show);

    // The HMONITOR of the one display the guest sees, the same on every call.
    std::uint32_t PrimaryMonitor();

    // The calling thread's active and keyboard-focus windows, 0 for none.
    std::uint32_t active_window() const { return active_window_; }
    void set_active_window(std::uint32_t window) { active_window_ = window; }
    std::uint32_t focus_window() const { return focus_window_; }
    void set_focus_window(std::uint32_t window) { focus_window_ = window; }

private:
    std::uint32_t AllocateHandle();

    std::uint32_t next_handle_ = 0x00010010U;
    std::uint16_t next_atom_ = kFirstClassAtom;
    std::map<std::uint32_t, std::uint32_t> system_icons_;
    std::map<std::uint32_t, std::uint32_t> system_cursors_;
    std::map<std::uint16_t, GuestWindowClass> classes_;
    std::map<std::uint32_t, GuestWindow> windows_;
    std::uint32_t active_window_ = 0;
    std::uint32_t focus_window_ = 0;
    std::uint32_t primary_monitor_ = 0;
    std::int32_t cursor_count_ = 0;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_USER_H_
