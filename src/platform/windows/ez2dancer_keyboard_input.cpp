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
    // The key this binding carries with no configuration file, written the way
    // an INI would write it so both go through the same interpretation. These
    // match config/ez2dancer-io.example.ini, which a unit test enforces.
    const char* default_key;
};

constexpr ButtonBinding kButtonBindings[] = {
    {"p1_left", re2dj::input::Ez2DancerButton::kPlayer1Left, "Q"},
    {"p1_center", re2dj::input::Ez2DancerButton::kPlayer1Centre, "S"},
    {"p1_right", re2dj::input::Ez2DancerButton::kPlayer1Right, "R"},
    {"p2_left", re2dj::input::Ez2DancerButton::kPlayer2Left, "NUMPAD1"},
    {"p2_center", re2dj::input::Ez2DancerButton::kPlayer2Centre, "NUMPAD2"},
    {"p2_right", re2dj::input::Ez2DancerButton::kPlayer2Right, "NUMPAD3"},
    {"p1_sensor_top_left", re2dj::input::Ez2DancerButton::kPlayer1SensorTopLeft, "W"},
    {"p1_sensor_top_right", re2dj::input::Ez2DancerButton::kPlayer1SensorTopRight, "E"},
    {"p1_sensor_bottom_left", re2dj::input::Ez2DancerButton::kPlayer1SensorBottomLeft, "A"},
    {"p1_sensor_bottom_right", re2dj::input::Ez2DancerButton::kPlayer1SensorBottomRight, "D"},
    {"p2_sensor_top_left", re2dj::input::Ez2DancerButton::kPlayer2SensorTopLeft, "U"},
    {"p2_sensor_top_right", re2dj::input::Ez2DancerButton::kPlayer2SensorTopRight, "I"},
    {"p2_sensor_bottom_left", re2dj::input::Ez2DancerButton::kPlayer2SensorBottomLeft, "J"},
    {"p2_sensor_bottom_right", re2dj::input::Ez2DancerButton::kPlayer2SensorBottomRight, "K"},
    {"coin", re2dj::input::Ez2DancerButton::kCoin, "F5"},
    {"test", re2dj::input::Ez2DancerButton::kTest, "F1"},
    {"service", re2dj::input::Ez2DancerButton::kService, "F2"},
};

}  // namespace

bool Ez2DancerKeyboardInput::Initialize(const char* path, std::string* error)
{
    if (error == nullptr)
    {
        return false;
    }
    const bool has_file = path != nullptr && path[0] != '\0';
    for (const ButtonBinding& binding : kButtonBindings)
    {
        int key = 0;
        if (!ParseKeyboardKeyName(binding.default_key, &key))
        {
            *error = std::string("built-in default is not a key name: ") + binding.default_key;
            return false;
        }
        bool present = false;
        if (has_file &&
            !ReadKeyboardKeyBinding(path, "buttons", binding.name, &key, &present, error))
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
