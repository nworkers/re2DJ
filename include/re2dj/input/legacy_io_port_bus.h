#ifndef RE2DJ_INPUT_LEGACY_IO_PORT_BUS_H_
#define RE2DJ_INPUT_LEGACY_IO_PORT_BUS_H_

#include <array>
#include <cstdint>

#include "re2dj/input/ez2dj_io_board.h"

namespace re2dj::input
{

class LegacyIoPortBus
{
public:
    LegacyIoPortBus();

    bool ReadByte(std::uint16_t port, std::uint8_t* value);
    bool WriteByte(std::uint16_t port, std::uint8_t value);
    bool SetInputByte(std::uint16_t port, std::uint8_t value);
    bool ClearInputOverride(std::uint16_t port);
    bool GetLastOutputByte(std::uint16_t port, std::uint8_t* value) const;
    bool SetButton(Ez2DjButton button, bool pressed);
    bool SetTurntable(Ez2DjPlayer player, std::uint8_t position);
    bool GetLight(Ez2DjLight light, bool* enabled) const;

private:
    static constexpr std::uint16_t kFirstPort = 0x100;
    static constexpr std::uint16_t kLastPort = 0x106;
    static constexpr std::size_t kPortCount = kLastPort - kFirstPort + 1;

    Ez2DjIoBoard board_;
    std::array<std::uint8_t, kPortCount> input_overrides_ = {};
    std::array<bool, kPortCount> input_overridden_ = {};
    std::array<std::uint8_t, kPortCount> outputs_ = {};
    std::array<bool, kPortCount> output_written_ = {};
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_LEGACY_IO_PORT_BUS_H_
