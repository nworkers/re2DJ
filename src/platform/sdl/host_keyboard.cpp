#include "host_keyboard.h"

#include <SDL3/SDL_scancode.h>

#include "re2dj/input/virtual_keys.h"

namespace re2dj::platform::sdl
{
namespace
{

namespace input = re2dj::input;

struct KeyEntry
{
    SDL_Scancode scancode;
    int virtual_key;
    int scan_code;
};

// Scan codes are the PC set 1 codes DirectInput reports (dinput.h DIK_*);
// the extended keys carry 0x80.
constexpr KeyEntry kKeys[] = {
    {SDL_SCANCODE_A, 'A', 0x1E}, {SDL_SCANCODE_B, 'B', 0x30}, {SDL_SCANCODE_C, 'C', 0x2E},
    {SDL_SCANCODE_D, 'D', 0x20}, {SDL_SCANCODE_E, 'E', 0x12}, {SDL_SCANCODE_F, 'F', 0x21},
    {SDL_SCANCODE_G, 'G', 0x22}, {SDL_SCANCODE_H, 'H', 0x23}, {SDL_SCANCODE_I, 'I', 0x17},
    {SDL_SCANCODE_J, 'J', 0x24}, {SDL_SCANCODE_K, 'K', 0x25}, {SDL_SCANCODE_L, 'L', 0x26},
    {SDL_SCANCODE_M, 'M', 0x32}, {SDL_SCANCODE_N, 'N', 0x31}, {SDL_SCANCODE_O, 'O', 0x18},
    {SDL_SCANCODE_P, 'P', 0x19}, {SDL_SCANCODE_Q, 'Q', 0x10}, {SDL_SCANCODE_R, 'R', 0x13},
    {SDL_SCANCODE_S, 'S', 0x1F}, {SDL_SCANCODE_T, 'T', 0x14}, {SDL_SCANCODE_U, 'U', 0x16},
    {SDL_SCANCODE_V, 'V', 0x2F}, {SDL_SCANCODE_W, 'W', 0x11}, {SDL_SCANCODE_X, 'X', 0x2D},
    {SDL_SCANCODE_Y, 'Y', 0x15}, {SDL_SCANCODE_Z, 'Z', 0x2C},
    {SDL_SCANCODE_1, '1', 0x02}, {SDL_SCANCODE_2, '2', 0x03}, {SDL_SCANCODE_3, '3', 0x04},
    {SDL_SCANCODE_4, '4', 0x05}, {SDL_SCANCODE_5, '5', 0x06}, {SDL_SCANCODE_6, '6', 0x07},
    {SDL_SCANCODE_7, '7', 0x08}, {SDL_SCANCODE_8, '8', 0x09}, {SDL_SCANCODE_9, '9', 0x0A},
    {SDL_SCANCODE_0, '0', 0x0B},
    {SDL_SCANCODE_RETURN, input::kVkReturn, 0x1C}, {SDL_SCANCODE_ESCAPE, input::kVkEscape, 0x01},
    {SDL_SCANCODE_BACKSPACE, input::kVkBack, 0x0E}, {SDL_SCANCODE_TAB, input::kVkTab, 0x0F},
    {SDL_SCANCODE_SPACE, input::kVkSpace, 0x39}, {SDL_SCANCODE_MINUS, input::kVkOemMinus, 0x0C},
    {SDL_SCANCODE_EQUALS, input::kVkOemPlus, 0x0D}, {SDL_SCANCODE_LEFTBRACKET, input::kVkOem4, 0x1A},
    {SDL_SCANCODE_RIGHTBRACKET, input::kVkOem6, 0x1B}, {SDL_SCANCODE_BACKSLASH, input::kVkOem5, 0x2B},
    {SDL_SCANCODE_SEMICOLON, input::kVkOem1, 0x27}, {SDL_SCANCODE_APOSTROPHE, input::kVkOem7, 0x28},
    {SDL_SCANCODE_GRAVE, input::kVkOem3, 0x29}, {SDL_SCANCODE_COMMA, input::kVkOemComma, 0x33},
    {SDL_SCANCODE_PERIOD, input::kVkOemPeriod, 0x34}, {SDL_SCANCODE_SLASH, input::kVkOem2, 0x35},
    {SDL_SCANCODE_CAPSLOCK, input::kVkCapital, 0x3A},
    {SDL_SCANCODE_F1, input::kVkF1 + 0, 0x3B}, {SDL_SCANCODE_F2, input::kVkF1 + 1, 0x3C},
    {SDL_SCANCODE_F3, input::kVkF1 + 2, 0x3D}, {SDL_SCANCODE_F4, input::kVkF1 + 3, 0x3E},
    {SDL_SCANCODE_F5, input::kVkF1 + 4, 0x3F}, {SDL_SCANCODE_F6, input::kVkF1 + 5, 0x40},
    {SDL_SCANCODE_F7, input::kVkF1 + 6, 0x41}, {SDL_SCANCODE_F8, input::kVkF1 + 7, 0x42},
    {SDL_SCANCODE_F9, input::kVkF1 + 8, 0x43}, {SDL_SCANCODE_F10, input::kVkF1 + 9, 0x44},
    {SDL_SCANCODE_F11, input::kVkF1 + 10, 0x57}, {SDL_SCANCODE_F12, input::kVkF1 + 11, 0x58},
    {SDL_SCANCODE_SCROLLLOCK, input::kVkScroll, 0x46}, {SDL_SCANCODE_PAUSE, input::kVkPause, 0xC5},
    {SDL_SCANCODE_INSERT, input::kVkInsert, 0xD2}, {SDL_SCANCODE_HOME, input::kVkHome, 0xC7},
    {SDL_SCANCODE_PAGEUP, input::kVkPrior, 0xC9}, {SDL_SCANCODE_DELETE, input::kVkDelete, 0xD3},
    {SDL_SCANCODE_END, input::kVkEnd, 0xCF}, {SDL_SCANCODE_PAGEDOWN, input::kVkNext, 0xD1},
    {SDL_SCANCODE_RIGHT, input::kVkRight, 0xCD}, {SDL_SCANCODE_LEFT, input::kVkLeft, 0xCB},
    {SDL_SCANCODE_DOWN, input::kVkDown, 0xD0}, {SDL_SCANCODE_UP, input::kVkUp, 0xC8},
    {SDL_SCANCODE_NUMLOCKCLEAR, input::kVkNumLock, 0x45}, {SDL_SCANCODE_KP_DIVIDE, input::kVkDivide, 0xB5},
    {SDL_SCANCODE_KP_MULTIPLY, input::kVkMultiply, 0x37}, {SDL_SCANCODE_KP_MINUS, input::kVkSubtract, 0x4A},
    {SDL_SCANCODE_KP_PLUS, input::kVkAdd, 0x4E}, {SDL_SCANCODE_KP_ENTER, input::kVkReturn, 0x9C},
    {SDL_SCANCODE_KP_1, input::kVkNumpad0 + 1, 0x4F}, {SDL_SCANCODE_KP_2, input::kVkNumpad0 + 2, 0x50},
    {SDL_SCANCODE_KP_3, input::kVkNumpad0 + 3, 0x51}, {SDL_SCANCODE_KP_4, input::kVkNumpad0 + 4, 0x4B},
    {SDL_SCANCODE_KP_5, input::kVkNumpad0 + 5, 0x4C}, {SDL_SCANCODE_KP_6, input::kVkNumpad0 + 6, 0x4D},
    {SDL_SCANCODE_KP_7, input::kVkNumpad0 + 7, 0x47}, {SDL_SCANCODE_KP_8, input::kVkNumpad0 + 8, 0x48},
    {SDL_SCANCODE_KP_9, input::kVkNumpad0 + 9, 0x49}, {SDL_SCANCODE_KP_0, input::kVkNumpad0, 0x52},
    {SDL_SCANCODE_KP_PERIOD, input::kVkDecimal, 0x53},
    {SDL_SCANCODE_LCTRL, input::kVkLControl, 0x1D}, {SDL_SCANCODE_LSHIFT, input::kVkLShift, 0x2A},
    {SDL_SCANCODE_LALT, input::kVkLMenu, 0x38}, {SDL_SCANCODE_LGUI, input::kVkLWin, 0xDB},
    {SDL_SCANCODE_RCTRL, input::kVkRControl, 0x9D}, {SDL_SCANCODE_RSHIFT, input::kVkRShift, 0x36},
    {SDL_SCANCODE_RALT, input::kVkRMenu, 0xB8}, {SDL_SCANCODE_RGUI, input::kVkRWin, 0xDC},
};

// The generic key a left or right key also holds, or 0.
int GenericKey(int virtual_key)
{
    switch (virtual_key)
    {
    case input::kVkLShift:
    case input::kVkRShift:
        return input::kVkShift;
    case input::kVkLControl:
    case input::kVkRControl:
        return input::kVkControl;
    case input::kVkLMenu:
    case input::kVkRMenu:
        return input::kVkMenu;
    default:
        return 0;
    }
}

}  // namespace

GuestKey GuestKeyForScancode(int sdl_scancode)
{
    for (const KeyEntry& entry : kKeys)
    {
        if (entry.scancode == sdl_scancode)
        {
            return {entry.virtual_key, entry.scan_code};
        }
    }
    return {};
}

void SetHostKey(hle::HostInputState* state, int sdl_scancode, bool down)
{
    const GuestKey key = GuestKeyForScancode(sdl_scancode);
    if (key.virtual_key == 0)
    {
        return;
    }
    state->scan_codes.set(static_cast<std::size_t>(key.scan_code), down);
    // Keypad Enter shares VK_RETURN with the main Enter key, which stays held
    // while either is down.
    if (key.virtual_key == input::kVkReturn)
    {
        const bool either = state->scan_codes.test(0x1C) || state->scan_codes.test(0x9C);
        state->virtual_keys.set(input::kVkReturn, either);
        return;
    }
    state->virtual_keys.set(static_cast<std::size_t>(key.virtual_key), down);
    const int generic = GenericKey(key.virtual_key);
    if (generic != 0)
    {
        const int left = generic == input::kVkShift ? input::kVkLShift
                         : generic == input::kVkControl ? input::kVkLControl
                                                        : input::kVkLMenu;
        state->virtual_keys.set(static_cast<std::size_t>(generic),
                                state->virtual_keys.test(static_cast<std::size_t>(left)) ||
                                    state->virtual_keys.test(static_cast<std::size_t>(left + 1)));
    }
}

}  // namespace re2dj::platform::sdl
