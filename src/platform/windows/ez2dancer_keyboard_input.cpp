#include "ez2dancer_keyboard_input.h"

#include "keyboard_input_common.h"

namespace re2dj::platform::windows
{
namespace
{

struct ButtonBinding
{
    const char* name;
    re2dj::input::Ez2DancerButton button;
};

constexpr ButtonBinding kButtonBindings[] = {
    {"p1_left", re2dj::input::Ez2DancerButton::kPlayer1Left},
    {"p1_center", re2dj::input::Ez2DancerButton::kPlayer1Centre},
    {"p1_right", re2dj::input::Ez2DancerButton::kPlayer1Right},
    {"p2_left", re2dj::input::Ez2DancerButton::kPlayer2Left},
    {"p2_center", re2dj::input::Ez2DancerButton::kPlayer2Centre},
    {"p2_right", re2dj::input::Ez2DancerButton::kPlayer2Right},
    {"p1_sensor_top_left", re2dj::input::Ez2DancerButton::kPlayer1SensorTopLeft},
    {"p1_sensor_top_right", re2dj::input::Ez2DancerButton::kPlayer1SensorTopRight},
    {"p1_sensor_bottom_left", re2dj::input::Ez2DancerButton::kPlayer1SensorBottomLeft},
    {"p1_sensor_bottom_right", re2dj::input::Ez2DancerButton::kPlayer1SensorBottomRight},
    {"p2_sensor_top_left", re2dj::input::Ez2DancerButton::kPlayer2SensorTopLeft},
    {"p2_sensor_top_right", re2dj::input::Ez2DancerButton::kPlayer2SensorTopRight},
    {"p2_sensor_bottom_left", re2dj::input::Ez2DancerButton::kPlayer2SensorBottomLeft},
    {"p2_sensor_bottom_right", re2dj::input::Ez2DancerButton::kPlayer2SensorBottomRight},
    {"coin", re2dj::input::Ez2DancerButton::kCoin},
    {"test", re2dj::input::Ez2DancerButton::kTest},
    {"service", re2dj::input::Ez2DancerButton::kService},
};

}  // namespace

bool Ez2DancerKeyboardInput::Initialize(const char* path, std::string* error)
{
    if (path == nullptr || path[0] == '\0' || error == nullptr)
    {
        return false;
    }
    for (const ButtonBinding& binding : kButtonBindings)
    {
        int key = 0;
        if (!ReadKeyboardKeyBinding(path, "buttons", binding.name, &key, error))
        {
            return false;
        }
        button_keys_[static_cast<std::size_t>(binding.button)] = key;
    }
    error->clear();
    return true;
}

void Ez2DancerKeyboardInput::Poll(re2dj::input::Ez2DancerIoPortBus* bus)
{
    if (bus == nullptr)
    {
        return;
    }
    for (std::size_t index = 0; index < button_keys_.size(); ++index)
    {
        bus->SetButton(static_cast<re2dj::input::Ez2DancerButton>(index),
                       IsKeyboardKeyPressed(button_keys_[index]));
    }
}

}  // namespace re2dj::platform::windows
