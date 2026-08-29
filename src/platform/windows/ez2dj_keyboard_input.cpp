#define NOMINMAX
#include <windows.h>

#include "ez2dj_keyboard_input.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace re2dj::platform::windows
{
namespace
{

struct ButtonBinding
{
    const char* name;
    re2dj::input::Ez2DjButton button;
};

constexpr ButtonBinding kButtonBindings[] = {
    {"p1_start", re2dj::input::Ez2DjButton::kPlayer1Start},
    {"p2_start", re2dj::input::Ez2DjButton::kPlayer2Start},
    {"effector1", re2dj::input::Ez2DjButton::kEffector1},
    {"effector2", re2dj::input::Ez2DjButton::kEffector2},
    {"effector3", re2dj::input::Ez2DjButton::kEffector3},
    {"effector4", re2dj::input::Ez2DjButton::kEffector4},
    {"service", re2dj::input::Ez2DjButton::kService},
    {"test", re2dj::input::Ez2DjButton::kTest},
    {"coin", re2dj::input::Ez2DjButton::kCoin},
    {"p1_1", re2dj::input::Ez2DjButton::kPlayer1Key1},
    {"p1_2", re2dj::input::Ez2DjButton::kPlayer1Key2},
    {"p1_3", re2dj::input::Ez2DjButton::kPlayer1Key3},
    {"p1_4", re2dj::input::Ez2DjButton::kPlayer1Key4},
    {"p1_5", re2dj::input::Ez2DjButton::kPlayer1Key5},
    {"p1_pedal", re2dj::input::Ez2DjButton::kPlayer1Pedal},
    {"p2_1", re2dj::input::Ez2DjButton::kPlayer2Key1},
    {"p2_2", re2dj::input::Ez2DjButton::kPlayer2Key2},
    {"p2_3", re2dj::input::Ez2DjButton::kPlayer2Key3},
    {"p2_4", re2dj::input::Ez2DjButton::kPlayer2Key4},
    {"p2_5", re2dj::input::Ez2DjButton::kPlayer2Key5},
    {"p2_pedal", re2dj::input::Ez2DjButton::kPlayer2Pedal},
};

std::string Upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

int ParseKey(const std::string& input)
{
    const std::string value = Upper(input);
    if (value.empty() || value == "NONE") return 0;
    if (value.size() == 1 && ((value[0] >= 'A' && value[0] <= 'Z') ||
                              (value[0] >= '0' && value[0] <= '9'))) return value[0];
    if (value[0] == 'F' && value.size() <= 3)
    {
        const int number = std::atoi(value.c_str() + 1);
        if (number >= 1 && number <= 24) return VK_F1 + number - 1;
    }
    if (value.rfind("NUMPAD", 0) == 0 && value.size() == 7 &&
        value[6] >= '0' && value[6] <= '9') return VK_NUMPAD0 + value[6] - '0';
    if (value == "ENTER") return VK_RETURN;
    if (value == "SPACE") return VK_SPACE;
    if (value == "LSHIFT") return VK_LSHIFT;
    if (value == "RSHIFT") return VK_RSHIFT;
    if (value == "LEFT") return VK_LEFT;
    if (value == "RIGHT") return VK_RIGHT;
    if (value == "UP") return VK_UP;
    if (value == "DOWN") return VK_DOWN;
    if (value == "DECIMAL") return VK_DECIMAL;
    return -1;
}

bool ReadKey(const char* path, const char* section, const char* name, int* key, std::string* error)
{
    char value[32] = {};
    GetPrivateProfileStringA(section, name, "NONE", value, sizeof(value), path);
    *key = ParseKey(value);
    if (*key >= 0) return true;
    *error = std::string("unknown key name for ") + section + "." + name + ": " + value;
    return false;
}

bool IsPressed(int key)
{
    return key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

}  // namespace

bool Ez2DjKeyboardInput::Initialize(const char* path, std::string* error)
{
    if (path == nullptr || path[0] == '\0' || error == nullptr)
    {
        return false;
    }
    for (const ButtonBinding& binding : kButtonBindings)
    {
        int key = 0;
        if (!ReadKey(path, "buttons", binding.name, &key, error)) return false;
        button_keys_[static_cast<std::size_t>(binding.button)] = key;
    }
    constexpr const char* kTurntableNames[] = {
        "p1_negative", "p1_positive", "p2_negative", "p2_positive"};
    for (std::size_t index = 0; index < turntable_keys_.size(); ++index)
    {
        if (!ReadKey(path, "turntables", kTurntableNames[index],
                     &turntable_keys_[index], error)) return false;
    }
    const UINT step = GetPrivateProfileIntA("turntables", "step", 4, path);
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
        bus->SetButton(static_cast<re2dj::input::Ez2DjButton>(index), IsPressed(button_keys_[index]));
    }
    if (last_turntable_update_ms_ != 0 && now_ms - last_turntable_update_ms_ < 8) return;
    last_turntable_update_ms_ = now_ms;
    for (std::size_t player = 0; player < 2; ++player)
    {
        const int direction = static_cast<int>(IsPressed(turntable_keys_[player * 2 + 1])) -
                              static_cast<int>(IsPressed(turntable_keys_[player * 2]));
        turntable_positions_[player] = static_cast<std::uint8_t>(
            turntable_positions_[player] + direction * turntable_step_);
        bus->SetTurntable(static_cast<re2dj::input::Ez2DjPlayer>(player),
                          turntable_positions_[player]);
    }
}

}  // namespace re2dj::platform::windows
