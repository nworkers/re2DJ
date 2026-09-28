#ifndef RE2DJ_INPUT_EZ2DANCER_KEYBOARD_MAP_H_
#define RE2DJ_INPUT_EZ2DANCER_KEYBOARD_MAP_H_

#include <span>
#include <string_view>

#include "re2dj/input/ez2dancer_io_board.h"

// How keys stand in for the EZ2Dancer I/O board, for both hosts: each pad,
// sensor, and cabinet button has a name (the key an INI file binds it under)
// and a built-in default key.
namespace re2dj::input
{

struct Ez2DancerButtonBinding
{
    std::string_view name;
    Ez2DancerButton button;
    // Written as an INI would write it; config/ez2dancer-io.example.ini lists
    // the same keys, which a unit test enforces.
    std::string_view default_key;
};
std::span<const Ez2DancerButtonBinding> Ez2DancerButtonBindings();

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_EZ2DANCER_KEYBOARD_MAP_H_
