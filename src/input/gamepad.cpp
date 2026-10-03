#include "re2dj/input/gamepad.h"

#include <cctype>
#include <string>

namespace re2dj::input
{
namespace
{

struct ControlName
{
    GamepadControl control;
    std::string_view name;
};

constexpr ControlName kControlNames[] = {
    {GamepadControl::kSouth, "A"},
    {GamepadControl::kEast, "B"},
    {GamepadControl::kWest, "X"},
    {GamepadControl::kNorth, "Y"},
    {GamepadControl::kBack, "BACK"},
    {GamepadControl::kGuide, "GUIDE"},
    {GamepadControl::kStart, "START"},
    {GamepadControl::kLeftStick, "LSTICK"},
    {GamepadControl::kRightStick, "RSTICK"},
    {GamepadControl::kLeftShoulder, "LB"},
    {GamepadControl::kRightShoulder, "RB"},
    {GamepadControl::kDpadUp, "DPAD_UP"},
    {GamepadControl::kDpadDown, "DPAD_DOWN"},
    {GamepadControl::kDpadLeft, "DPAD_LEFT"},
    {GamepadControl::kDpadRight, "DPAD_RIGHT"},
    {GamepadControl::kLeftTrigger, "LT"},
    {GamepadControl::kRightTrigger, "RT"},
    {GamepadControl::kPaddle1, "PADDLE1"},
    {GamepadControl::kPaddle2, "PADDLE2"},
    {GamepadControl::kPaddle3, "PADDLE3"},
    {GamepadControl::kPaddle4, "PADDLE4"},
    {GamepadControl::kLeftStickLeft, "LSTICK_LEFT"},
    {GamepadControl::kLeftStickRight, "LSTICK_RIGHT"},
    {GamepadControl::kLeftStickUp, "LSTICK_UP"},
    {GamepadControl::kLeftStickDown, "LSTICK_DOWN"},
    {GamepadControl::kRightStickLeft, "RSTICK_LEFT"},
    {GamepadControl::kRightStickRight, "RSTICK_RIGHT"},
    {GamepadControl::kRightStickUp, "RSTICK_UP"},
    {GamepadControl::kRightStickDown, "RSTICK_DOWN"},
};
static_assert(sizeof(kControlNames) / sizeof(kControlNames[0]) == kGamepadControlCount - 1);

}  // namespace

int ParseGamepadControlName(std::string_view name)
{
    std::string value(name);
    for (char& character : value)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    if (value.empty() || value == "NONE") return 0;
    for (const ControlName& entry : kControlNames)
    {
        if (value == entry.name) return static_cast<int>(entry.control);
    }
    return -1;
}

std::string_view GamepadControlName(GamepadControl control)
{
    for (const ControlName& entry : kControlNames)
    {
        if (entry.control == control) return entry.name;
    }
    return {};
}

}  // namespace re2dj::input
