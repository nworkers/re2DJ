#include "re2dj/input/ez2dj_keyboard_map.h"

namespace re2dj::input
{
namespace
{

constexpr Ez2DjButtonBinding kButtonBindings[] = {
    {"p1_start", Ez2DjButton::kPlayer1Start, "1"},
    {"p2_start", Ez2DjButton::kPlayer2Start, "2"},
    {"effector1", Ez2DjButton::kEffector1, "Q"},
    {"effector2", Ez2DjButton::kEffector2, "W"},
    {"effector3", Ez2DjButton::kEffector3, "E"},
    {"effector4", Ez2DjButton::kEffector4, "R"},
    {"service", Ez2DjButton::kService, "F2"},
    {"test", Ez2DjButton::kTest, "F1"},
    {"coin", Ez2DjButton::kCoin, "F5"},
    {"p1_1", Ez2DjButton::kPlayer1Key1, "Z"},
    {"p1_2", Ez2DjButton::kPlayer1Key2, "S"},
    {"p1_3", Ez2DjButton::kPlayer1Key3, "X"},
    {"p1_4", Ez2DjButton::kPlayer1Key4, "D"},
    {"p1_5", Ez2DjButton::kPlayer1Key5, "C"},
    {"p1_pedal", Ez2DjButton::kPlayer1Pedal, "SPACE"},
    {"p2_1", Ez2DjButton::kPlayer2Key1, "NUMPAD1"},
    {"p2_2", Ez2DjButton::kPlayer2Key2, "NUMPAD2"},
    {"p2_3", Ez2DjButton::kPlayer2Key3, "NUMPAD3"},
    {"p2_4", Ez2DjButton::kPlayer2Key4, "NUMPAD4"},
    {"p2_5", Ez2DjButton::kPlayer2Key5, "NUMPAD5"},
    {"p2_pedal", Ez2DjButton::kPlayer2Pedal, "DECIMAL"},
};

constexpr Ez2DjTurntableBinding kTurntableBindings[] = {
    {"p1_negative", "LSHIFT"},
    {"p1_positive", "TAB"},
    {"p2_negative", "ENTER"},
    {"p2_positive", "RSHIFT"},
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
