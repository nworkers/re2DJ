#ifndef RE2DJ_INPUT_LEGACY_IO_PORT_BUS_H_
#define RE2DJ_INPUT_LEGACY_IO_PORT_BUS_H_

#include <array>
#include <cstdint>

namespace re2dj::input
{

class LegacyIoPortBus
{
public:
    LegacyIoPortBus();

    bool ReadByte(std::uint16_t port, std::uint8_t* value) const;
    bool WriteByte(std::uint16_t port, std::uint8_t value);
    bool SetInputByte(std::uint16_t port, std::uint8_t value);
    bool GetLastOutputByte(std::uint16_t port, std::uint8_t* value) const;

private:
    static constexpr std::uint16_t kFirstPort = 0x100;
    static constexpr std::uint16_t kLastPort = 0x106;
    static constexpr std::size_t kPortCount = kLastPort - kFirstPort + 1;

    std::array<std::uint8_t, kPortCount> inputs_ = {};
    std::array<std::uint8_t, kPortCount> outputs_ = {};
    std::array<bool, kPortCount> output_written_ = {};
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_LEGACY_IO_PORT_BUS_H_
