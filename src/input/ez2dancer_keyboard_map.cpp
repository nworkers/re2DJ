#include "re2dj/input/ez2dancer_keyboard_map.h"

namespace re2dj::input
{
namespace
{

constexpr Ez2DancerButtonBinding kButtonBindings[] = {
    {"p1_left", Ez2DancerButton::kPlayer1Left, "Q"},
    {"p1_center", Ez2DancerButton::kPlayer1Centre, "S"},
    {"p1_right", Ez2DancerButton::kPlayer1Right, "R"},
    {"p2_left", Ez2DancerButton::kPlayer2Left, "NUMPAD1"},
    {"p2_center", Ez2DancerButton::kPlayer2Centre, "NUMPAD2"},
    {"p2_right", Ez2DancerButton::kPlayer2Right, "NUMPAD3"},
    {"p1_sensor_top_left", Ez2DancerButton::kPlayer1SensorTopLeft, "W"},
    {"p1_sensor_top_right", Ez2DancerButton::kPlayer1SensorTopRight, "E"},
    {"p1_sensor_bottom_left", Ez2DancerButton::kPlayer1SensorBottomLeft, "A"},
    {"p1_sensor_bottom_right", Ez2DancerButton::kPlayer1SensorBottomRight, "D"},
    {"p2_sensor_top_left", Ez2DancerButton::kPlayer2SensorTopLeft, "U"},
    {"p2_sensor_top_right", Ez2DancerButton::kPlayer2SensorTopRight, "I"},
    {"p2_sensor_bottom_left", Ez2DancerButton::kPlayer2SensorBottomLeft, "J"},
    {"p2_sensor_bottom_right", Ez2DancerButton::kPlayer2SensorBottomRight, "K"},
    {"coin", Ez2DancerButton::kCoin, "F5"},
    {"test", Ez2DancerButton::kTest, "F1"},
    {"service", Ez2DancerButton::kService, "F2"},
};

}  // namespace

std::span<const Ez2DancerButtonBinding> Ez2DancerButtonBindings()
{
    return kButtonBindings;
}

}  // namespace re2dj::input
