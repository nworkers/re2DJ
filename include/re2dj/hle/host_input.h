#ifndef RE2DJ_HLE_HOST_INPUT_H_
#define RE2DJ_HLE_HOST_INPUT_H_

#include <array>
#include <bitset>
#include <cstdint>

namespace re2dj::hle
{

// What the host's keyboard and mouse hold, as the guest's input APIs read it:
// keys both by Win32 virtual-key code (GetAsyncKeyState, key bindings; the
// mouse buttons included at VK_LBUTTON, VK_RBUTTON and VK_MBUTTON, and a
// left or right shift, control or alt key also holding the generic one) and
// by DirectInput scan code (DIK_*), and where the pointer is.
struct HostInputState
{
    std::bitset<256> virtual_keys;
    std::bitset<256> scan_codes;
    // Left, right, middle.
    std::array<bool, 3> mouse_buttons{};
    // The pointer over the guest window, in the guest display's coordinates;
    // cursor_window is that window, 0 until the pointer has been over it.
    std::uint32_t cursor_window = 0;
    std::int32_t cursor_x = 0;
    std::int32_t cursor_y = 0;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_HOST_INPUT_H_
