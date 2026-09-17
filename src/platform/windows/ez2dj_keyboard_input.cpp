#define NOMINMAX
#include <windows.h>

#include "ez2dj_keyboard_input.h"
#include "keyboard_input_common.h"

#include <string>

namespace re2dj::platform::windows
{
namespace
{

struct ButtonBinding
{
    const char* name;
    re2dj::input::Ez2DjButton button;
    // The key this binding carries with no configuration file, written the way
    // an INI would write it so both go through the same interpretation. These
    // match config/ez2dj-io.example.ini, which a unit test enforces.
    const char* default_key;
};

constexpr ButtonBinding kButtonBindings[] = {
    {"p1_start", re2dj::input::Ez2DjButton::kPlayer1Start, "1"},
    {"p2_start", re2dj::input::Ez2DjButton::kPlayer2Start, "2"},
    {"effector1", re2dj::input::Ez2DjButton::kEffector1, "Q"},
    {"effector2", re2dj::input::Ez2DjButton::kEffector2, "W"},
    {"effector3", re2dj::input::Ez2DjButton::kEffector3, "E"},
    {"effector4", re2dj::input::Ez2DjButton::kEffector4, "R"},
    {"service", re2dj::input::Ez2DjButton::kService, "F2"},
    {"test", re2dj::input::Ez2DjButton::kTest, "F1"},
    {"coin", re2dj::input::Ez2DjButton::kCoin, "F5"},
    {"p1_1", re2dj::input::Ez2DjButton::kPlayer1Key1, "Z"},
    {"p1_2", re2dj::input::Ez2DjButton::kPlayer1Key2, "S"},
    {"p1_3", re2dj::input::Ez2DjButton::kPlayer1Key3, "X"},
    {"p1_4", re2dj::input::Ez2DjButton::kPlayer1Key4, "D"},
    {"p1_5", re2dj::input::Ez2DjButton::kPlayer1Key5, "C"},
    {"p1_pedal", re2dj::input::Ez2DjButton::kPlayer1Pedal, "SPACE"},
    {"p2_1", re2dj::input::Ez2DjButton::kPlayer2Key1, "NUMPAD1"},
    {"p2_2", re2dj::input::Ez2DjButton::kPlayer2Key2, "NUMPAD2"},
    {"p2_3", re2dj::input::Ez2DjButton::kPlayer2Key3, "NUMPAD3"},
    {"p2_4", re2dj::input::Ez2DjButton::kPlayer2Key4, "NUMPAD4"},
    {"p2_5", re2dj::input::Ez2DjButton::kPlayer2Key5, "NUMPAD5"},
    {"p2_pedal", re2dj::input::Ez2DjButton::kPlayer2Pedal, "DECIMAL"},
};

struct TurntableBinding
{
    const char* name;
    const char* default_key;
};

constexpr TurntableBinding kTurntableBindings[] = {
    {"p1_negative", "LSHIFT"},
    {"p1_positive", "TAB"},
    {"p2_negative", "ENTER"},
    {"p2_positive", "RSHIFT"},
};

constexpr std::uint8_t kDefaultTurntableStep = 4;

}  // namespace

bool Ez2DjKeyboardInput::Initialize(const char* path, std::string* error)
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
    for (std::size_t index = 0; index < turntable_keys_.size(); ++index)
    {
        const TurntableBinding& binding = kTurntableBindings[index];
        int key = 0;
        if (!ParseKeyboardKeyName(binding.default_key, &key))
        {
            *error = std::string("built-in default is not a key name: ") + binding.default_key;
            return false;
        }
        bool present = false;
        if (has_file &&
            !ReadKeyboardKeyBinding(path, "turntables", binding.name, &key, &present, error))
        {
            return false;
        }
        turntable_keys_[index] = key;
    }
    UINT step = kDefaultTurntableStep;
    if (has_file)
    {
        step = GetPrivateProfileIntA("turntables", "step", kDefaultTurntableStep, path);
    }
    if (step < 1 || step > 32)
    {
        *error = "turntables.step must be between 1 and 32";
        return false;
    }
    turntable_step_ = static_cast<std::uint8_t>(step);
    error->clear();
    return true;
}

void Ez2DjKeyboardInput::Poll(re2dj::input::LegacyIoPortBus* bus, std::uint64_t now_ms)
{
    if (bus == nullptr) return;
    for (std::size_t index = 0; index < button_keys_.size(); ++index)
    {
        bus->SetButton(static_cast<re2dj::input::Ez2DjButton>(index),
                       IsKeyboardKeyPressed(button_keys_[index]));
    }
    if (last_turntable_update_ms_ != 0 && now_ms - last_turntable_update_ms_ < 8) return;
    last_turntable_update_ms_ = now_ms;
    for (std::size_t player = 0; player < 2; ++player)
    {
        const int direction =
            static_cast<int>(IsKeyboardKeyPressed(turntable_keys_[player * 2 + 1])) -
            static_cast<int>(IsKeyboardKeyPressed(turntable_keys_[player * 2]));
        turntable_positions_[player] = static_cast<std::uint8_t>(
            turntable_positions_[player] + direction * turntable_step_);
        bus->SetTurntable(static_cast<re2dj::input::Ez2DjPlayer>(player),
                          turntable_positions_[player]);
    }
}

}  // namespace re2dj::platform::windows
