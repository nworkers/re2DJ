#include "re2dj/input/ez2dj_io_board.h"

namespace re2dj::input
{
namespace
{

std::size_t ToIndex(Ez2DjButton button)
{
    return static_cast<std::size_t>(button);
}

std::size_t ToIndex(Ez2DjLight light)
{
    return static_cast<std::size_t>(light);
}

void SetActiveLow(std::uint8_t bit, bool pressed, std::uint8_t* value)
{
    if (pressed)
    {
        *value &= static_cast<std::uint8_t>(~(1u << bit));
    }
}

}  // namespace

bool Ez2DjIoBoard::SetButton(Ez2DjButton button, bool pressed)
{
    const std::size_t index = ToIndex(button);
    if (index >= buttons_.size())
    {
        return false;
    }
    if (button == Ez2DjButton::kCoin && pressed && !buttons_[index])
    {
        ++coin_counter_;
    }
    buttons_[index] = pressed;
    return true;
}

bool Ez2DjIoBoard::SetTurntable(Ez2DjPlayer player, std::uint8_t position)
{
    const std::size_t index = static_cast<std::size_t>(player);
    if (index >= turntables_.size())
    {
        return false;
    }
    turntables_[index] = position;
    return true;
}

bool Ez2DjIoBoard::ReadPort(std::uint16_t port, std::uint8_t* value)
{
    if (value == nullptr)
    {
        return false;
    }
    if (port == 0x103 || port == 0x104)
    {
        *value = turntables_[port - 0x103];
        return true;
    }
    if (port == 0x105)
    {
        *value = coin_counter_;
        return true;
    }
    if (port < 0x101 || port > 0x106)
    {
        return false;
    }

    *value = 0xff;
    if (port == 0x101)
    {
        const Ez2DjButton mapping[] = {
            Ez2DjButton::kPlayer1Start, Ez2DjButton::kPlayer2Start,
            Ez2DjButton::kEffector1, Ez2DjButton::kEffector2,
            Ez2DjButton::kEffector3, Ez2DjButton::kEffector4,
            Ez2DjButton::kService, Ez2DjButton::kTest,
        };
        for (std::uint8_t bit = 0; bit < 8; ++bit)
        {
            SetActiveLow(bit, buttons_[ToIndex(mapping[bit])], value);
        }
        return true;
    }

    const bool player1 = port == 0x102;
    const Ez2DjButton first = player1 ? Ez2DjButton::kPlayer1Key1 : Ez2DjButton::kPlayer2Key1;
    for (std::uint8_t bit = 0; bit < 5; ++bit)
    {
        const auto button = static_cast<Ez2DjButton>(ToIndex(first) + bit);
        SetActiveLow(bit, buttons_[ToIndex(button)], value);
    }
    const Ez2DjButton pedal = player1 ? Ez2DjButton::kPlayer1Pedal : Ez2DjButton::kPlayer2Pedal;
    SetActiveLow(7, buttons_[ToIndex(pedal)], value);
    return true;
}

bool Ez2DjIoBoard::WritePort(std::uint16_t port, std::uint8_t value)
{
    Ez2DjLight first;
    std::uint8_t count = 0;
    switch (port)
    {
        case 0x100: first = Ez2DjLight::kRedLeft; count = 5; break;
        case 0x101: first = Ez2DjLight::kPlayer1Start; count = 6; break;
        case 0x102: first = Ez2DjLight::kPlayer1Key1; count = 6; break;
        case 0x103: first = Ez2DjLight::kPlayer2Key1; count = 6; break;
        default: return false;
    }
    for (std::uint8_t bit = 0; bit < count; ++bit)
    {
        lights_[ToIndex(first) + bit] = (value & (1u << bit)) != 0;
    }
    return true;
}

bool Ez2DjIoBoard::GetLight(Ez2DjLight light, bool* enabled) const
{
    const std::size_t index = ToIndex(light);
    if (enabled == nullptr || index >= lights_.size())
    {
        return false;
    }
    *enabled = lights_[index];
    return true;
}

}  // namespace re2dj::input
