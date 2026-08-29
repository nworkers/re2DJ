#include "re2dj/input/legacy_io_port_bus.h"

namespace re2dj::input
{

namespace
{

bool IsReadable(std::uint16_t port)
{
    return port >= 0x101 && port <= 0x106;
}

bool IsWritable(std::uint16_t port)
{
    return (port >= 0x100 && port <= 0x103) || port == 0x106;
}

std::size_t PortIndex(std::uint16_t port)
{
    return static_cast<std::size_t>(port - 0x100);
}

}  // namespace

LegacyIoPortBus::LegacyIoPortBus() = default;

bool LegacyIoPortBus::ReadByte(std::uint16_t port, std::uint8_t* value)
{
    if (value == nullptr || !IsReadable(port))
    {
        return false;
    }
    const std::size_t index = PortIndex(port);
    if (input_overridden_[index])
    {
        *value = input_overrides_[index];
        return true;
    }
    return board_.ReadPort(port, value);
}

bool LegacyIoPortBus::WriteByte(std::uint16_t port, std::uint8_t value)
{
    if (!IsWritable(port))
    {
        return false;
    }
    const std::size_t index = PortIndex(port);
    outputs_[index] = value;
    output_written_[index] = true;
    board_.WritePort(port, value);
    return true;
}

bool LegacyIoPortBus::SetInputByte(std::uint16_t port, std::uint8_t value)
{
    if (!IsReadable(port))
    {
        return false;
    }
    const std::size_t index = PortIndex(port);
    input_overrides_[index] = value;
    input_overridden_[index] = true;
    return true;
}

bool LegacyIoPortBus::ClearInputOverride(std::uint16_t port)
{
    if (!IsReadable(port))
    {
        return false;
    }
    input_overridden_[PortIndex(port)] = false;
    return true;
}

bool LegacyIoPortBus::GetLastOutputByte(std::uint16_t port, std::uint8_t* value) const
{
    if (value == nullptr || !IsWritable(port))
    {
        return false;
    }
    const std::size_t index = PortIndex(port);
    if (!output_written_[index])
    {
        return false;
    }
    *value = outputs_[index];
    return true;
}

bool LegacyIoPortBus::SetButton(Ez2DjButton button, bool pressed)
{
    return board_.SetButton(button, pressed);
}

bool LegacyIoPortBus::SetTurntable(Ez2DjPlayer player, std::uint8_t position)
{
    return board_.SetTurntable(player, position);
}

bool LegacyIoPortBus::GetLight(Ez2DjLight light, bool* enabled) const
{
    return board_.GetLight(light, enabled);
}

}  // namespace re2dj::input
