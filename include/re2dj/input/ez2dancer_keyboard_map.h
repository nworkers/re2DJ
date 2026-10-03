#ifndef RE2DJ_INPUT_EZ2DANCER_KEYBOARD_MAP_H_
#define RE2DJ_INPUT_EZ2DANCER_KEYBOARD_MAP_H_

#include <span>
#include <string_view>

#include "re2dj/input/ez2dancer_io_board.h"

// How keys and gamepad controls stand in for the EZ2Dancer I/O board, for
// both hosts: each pad, sensor, and cabinet button has a name (the key an INI
// file binds it under), a built-in default key and a built-in default gamepad
// control (task 444).
namespace re2dj::input
{

struct Ez2DancerButtonBinding
{
    std::string_view name;
    Ez2DancerButton button;
    // Written as an INI would write them; config/ez2dancer-io.example.ini
    // lists the same keys and controls, which a unit test enforces.
    std::string_view default_key;
    std::string_view default_gamepad;
};
std::span<const Ez2DancerButtonBinding> Ez2DancerButtonBindings();

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_EZ2DANCER_KEYBOARD_MAP_H_
