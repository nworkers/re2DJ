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

LegacyIoPortBus::LegacyIoPortBus()
{
    inputs_[PortIndex(0x101)] = 0xff;
    inputs_[PortIndex(0x102)] = 0xff;
    inputs_[PortIndex(0x106)] = 0xff;
}

bool LegacyIoPortBus::ReadByte(std::uint16_t port, std::uint8_t* value) const
{
    if (value == nullptr || !IsReadable(port))
    {
        return false;
    }
    *value = inputs_[PortIndex(port)];
    return true;
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
    return true;
}

bool LegacyIoPortBus::SetInputByte(std::uint16_t port, std::uint8_t value)
{
    if (!IsReadable(port))
    {
        return false;
    }
    inputs_[PortIndex(port)] = value;
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

}  // namespace re2dj::input
