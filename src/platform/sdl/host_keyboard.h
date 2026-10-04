#ifndef RE2DJ_PLATFORM_SDL_HOST_KEYBOARD_H_
#define RE2DJ_PLATFORM_SDL_HOST_KEYBOARD_H_

#include <cstdint>

#include "re2dj/hle/host_input.h"

namespace re2dj::platform::sdl
{

// A host key, by SDL scancode, as a Windows guest knows it on a US keyboard
// with Num Lock on: its virtual-key code and DirectInput scan code, 0 for a
// key the table does not carry.
struct GuestKey
{
    int virtual_key = 0;
    int scan_code = 0;
};
GuestKey GuestKeyForScancode(int sdl_scancode);

// Records a key going down or up in the input state: its virtual key and
// scan code, and the generic shift, control or alt key while either side of
// it is held.
void SetHostKey(hle::HostInputState* state, int sdl_scancode, bool down);

}  // namespace re2dj::platform::sdl

#endif  // RE2DJ_PLATFORM_SDL_HOST_KEYBOARD_H_
