#include "re2dj/input/ez2dancer_keyboard_map.h"

namespace re2dj::input
{
namespace
{

constexpr Ez2DancerButtonBinding kButtonBindings[] = {
    {"p1_left", Ez2DancerButton::kPlayer1Left, "Q", "DPAD_LEFT"},
    {"p1_center", Ez2DancerButton::kPlayer1Centre, "S", "DPAD_DOWN"},
    {"p1_right", Ez2DancerButton::kPlayer1Right, "R", "DPAD_RIGHT"},
    {"p2_left", Ez2DancerButton::kPlayer2Left, "NUMPAD1", "NONE"},
    {"p2_center", Ez2DancerButton::kPlayer2Centre, "NUMPAD2", "NONE"},
    {"p2_right", Ez2DancerButton::kPlayer2Right, "NUMPAD3", "NONE"},
    {"p1_sensor_top_left", Ez2DancerButton::kPlayer1SensorTopLeft, "W", "LB"},
    {"p1_sensor_top_right", Ez2DancerButton::kPlayer1SensorTopRight, "E", "RB"},
    {"p1_sensor_bottom_left", Ez2DancerButton::kPlayer1SensorBottomLeft, "A", "LT"},
    {"p1_sensor_bottom_right", Ez2DancerButton::kPlayer1SensorBottomRight, "D", "RT"},
    {"p2_sensor_top_left", Ez2DancerButton::kPlayer2SensorTopLeft, "U", "NONE"},
    {"p2_sensor_top_right", Ez2DancerButton::kPlayer2SensorTopRight, "I", "NONE"},
    {"p2_sensor_bottom_left", Ez2DancerButton::kPlayer2SensorBottomLeft, "J", "NONE"},
    {"p2_sensor_bottom_right", Ez2DancerButton::kPlayer2SensorBottomRight, "K", "NONE"},
    {"coin", Ez2DancerButton::kCoin, "F5", "BACK"},
    {"test", Ez2DancerButton::kTest, "F1", "NONE"},
    {"service", Ez2DancerButton::kService, "F2", "NONE"},
};

}  // namespace

std::span<const Ez2DancerButtonBinding> Ez2DancerButtonBindings()
{
    return kButtonBindings;
}

}  // namespace re2dj::input
