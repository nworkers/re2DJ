#include "re2dj/input/ez2dj_keyboard_map.h"

namespace re2dj::input
{
namespace
{

constexpr Ez2DjButtonBinding kButtonBindings[] = {
    {"p1_start", Ez2DjButton::kPlayer1Start, "1", "START"},
    {"p2_start", Ez2DjButton::kPlayer2Start, "2", "NONE"},
    {"effector1", Ez2DjButton::kEffector1, "Q", "DPAD_UP"},
    {"effector2", Ez2DjButton::kEffector2, "W", "DPAD_DOWN"},
    {"effector3", Ez2DjButton::kEffector3, "E", "DPAD_LEFT"},
    {"effector4", Ez2DjButton::kEffector4, "R", "DPAD_RIGHT"},
    {"service", Ez2DjButton::kService, "F2", "NONE"},
    {"test", Ez2DjButton::kTest, "F1", "NONE"},
    {"coin", Ez2DjButton::kCoin, "F5", "BACK"},
    {"p1_1", Ez2DjButton::kPlayer1Key1, "Z", "X"},
    {"p1_2", Ez2DjButton::kPlayer1Key2, "S", "Y"},
    {"p1_3", Ez2DjButton::kPlayer1Key3, "X", "B"},
    {"p1_4", Ez2DjButton::kPlayer1Key4, "D", "A"},
    {"p1_5", Ez2DjButton::kPlayer1Key5, "C", "RB"},
    {"p1_pedal", Ez2DjButton::kPlayer1Pedal, "SPACE", "LB"},
    {"p2_1", Ez2DjButton::kPlayer2Key1, "NUMPAD1", "NONE"},
    {"p2_2", Ez2DjButton::kPlayer2Key2, "NUMPAD2", "NONE"},
    {"p2_3", Ez2DjButton::kPlayer2Key3, "NUMPAD3", "NONE"},
    {"p2_4", Ez2DjButton::kPlayer2Key4, "NUMPAD4", "NONE"},
    {"p2_5", Ez2DjButton::kPlayer2Key5, "NUMPAD5", "NONE"},
    {"p2_pedal", Ez2DjButton::kPlayer2Pedal, "DECIMAL", "NONE"},
};

constexpr Ez2DjTurntableBinding kTurntableBindings[] = {
    {"p1_negative", "LSHIFT", "LSTICK_LEFT"},
    {"p1_positive", "TAB", "LSTICK_RIGHT"},
    {"p2_negative", "ENTER", "NONE"},
    {"p2_positive", "RSHIFT", "NONE"},
};

}  // namespace

std::span<const Ez2DjButtonBinding> Ez2DjButtonBindings()
{
    return kButtonBindings;
}

std::span<const Ez2DjTurntableBinding> Ez2DjTurntableBindings()
{
    return kTurntableBindings;
}

void Ez2DjTurntables::Update(LegacyIoPortBus* bus,
                             std::uint64_t now_ms,
                             const std::array<bool, 4>& held,
                             std::uint8_t step)
{
    if (bus == nullptr) return;
    if (last_update_ms_ != 0 && now_ms - last_update_ms_ < 8) return;
    last_update_ms_ = now_ms;
    for (std::size_t player = 0; player < 2; ++player)
    {
        const int direction = static_cast<int>(held[player * 2 + 1]) - static_cast<int>(held[player * 2]);
        positions_[player] = static_cast<std::uint8_t>(positions_[player] + direction * step);
        bus->SetTurntable(static_cast<Ez2DjPlayer>(player), positions_[player]);
    }
}

}  // namespace re2dj::input
