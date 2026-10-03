#ifndef RE2DJ_INPUT_GAMEPAD_H_
#define RE2DJ_INPUT_GAMEPAD_H_

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string_view>

// A gamepad's controls as the binding tables and INI files name them, in the
// layout SDL3 gives every gamepad (the Xbox shape), without SDL itself: the
// host's reader turns its device into this, and the I/O boards read it next
// to the keyboard. A stick direction or a trigger counts as held once it
// passes half of its travel.
namespace re2dj::input
{

enum class GamepadControl : std::uint8_t
{
    kNone = 0,
    // Face buttons, bottom, right, left, top: A, B, X, Y.
    kSouth,
    kEast,
    kWest,
    kNorth,
    kBack,
    kGuide,
    kStart,
    kLeftStick,
    kRightStick,
    kLeftShoulder,
    kRightShoulder,
    kDpadUp,
    kDpadDown,
    kDpadLeft,
    kDpadRight,
    kLeftTrigger,
    kRightTrigger,
    // PADDLE1..4: SDL's right paddle 1, left paddle 1, right paddle 2, left
    // paddle 2 (a Steam Deck's R4, L4, R5, L5).
    kPaddle1,
    kPaddle2,
    kPaddle3,
    kPaddle4,
    kLeftStickLeft,
    kLeftStickRight,
    kLeftStickUp,
    kLeftStickDown,
    kRightStickLeft,
    kRightStickRight,
    kRightStickUp,
    kRightStickDown,
    kCount,
};

inline constexpr std::size_t kGamepadControlCount = static_cast<std::size_t>(GamepadControl::kCount);
static_assert(kGamepadControlCount <= 32);

// What the host's gamepads hold, one bit per control's number.
using GamepadControls = std::bitset<32>;

// A control name as the binding tables and INI files write it
// (case-insensitive: A, B, X, Y, BACK, GUIDE, START, LSTICK, RSTICK, LB, RB,
// LT, RT, DPAD_UP/DOWN/LEFT/RIGHT, PADDLE1..4, LSTICK_LEFT/RIGHT/UP/DOWN and
// RSTICK_*): its number, 0 for NONE or an empty name (unbound), or -1 for a
// name it does not know. The contract is ParseKeyName's.
int ParseGamepadControlName(std::string_view name);
// The name the INI files write for a control; empty for kNone or kCount.
std::string_view GamepadControlName(GamepadControl control);

// Whether a control's number is held; 0 and out-of-range numbers are not.
inline bool GamepadControlHeld(const GamepadControls& controls, int control)
{
    return control > 0 && static_cast<std::size_t>(control) < kGamepadControlCount &&
           controls.test(static_cast<std::size_t>(control));
}

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_GAMEPAD_H_
