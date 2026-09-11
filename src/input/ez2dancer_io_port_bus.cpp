#include "re2dj/input/ez2dancer_io_port_bus.h"

namespace re2dj::input
{

Ez2DancerIoPortBus::Ez2DancerIoPortBus() = default;

bool Ez2DancerIoPortBus::PortIndex(std::uint16_t port, std::size_t* index)
{
    if (port < kFirstPort || port > kLastPort || (port & 1u) != 0)
    {
        return false;
    }
    *index = static_cast<std::size_t>((port - kFirstPort) / 2);
    return true;
}

bool Ez2DancerIoPortBus::ReadWord(std::uint16_t port, std::uint16_t* value)
{
    std::size_t index = 0;
    if (value == nullptr || !PortIndex(port, &index))
    {
        return false;
    }
    if (input_overridden_[index])
    {
        *value = input_overrides_[index];
        return true;
    }
    return board_.ReadPort(port, value);
}

bool Ez2DancerIoPortBus::WriteWord(std::uint16_t port, std::uint16_t value)
{
    std::size_t index = 0;
    if (!PortIndex(port, &index) || !board_.WritePort(port, value))
    {
        return false;
    }
    outputs_[index] = value;
    output_written_[index] = true;
    return true;
}

bool Ez2DancerIoPortBus::SetInputWord(std::uint16_t port, std::uint16_t value)
{
    std::size_t index = 0;
    if (!PortIndex(port, &index))
    {
        return false;
    }
    input_overrides_[index] = value;
    input_overridden_[index] = true;
    return true;
}

bool Ez2DancerIoPortBus::ClearInputOverride(std::uint16_t port)
{
    std::size_t index = 0;
    if (!PortIndex(port, &index))
    {
        return false;
    }
    input_overridden_[index] = false;
    return true;
}

bool Ez2DancerIoPortBus::GetLastOutputWord(std::uint16_t port, std::uint16_t* value) const
{
    std::size_t index = 0;
    if (value == nullptr || !PortIndex(port, &index) || !output_written_[index])
    {
        return false;
    }
    *value = outputs_[index];
    return true;
}

bool Ez2DancerIoPortBus::SetButton(Ez2DancerButton button, bool pressed)
{
    return board_.SetButton(button, pressed);
}

bool Ez2DancerIoPortBus::GetLight(Ez2DancerLight light, bool* enabled) const
{
    return board_.GetLight(light, enabled);
}

}  // namespace re2dj::input
