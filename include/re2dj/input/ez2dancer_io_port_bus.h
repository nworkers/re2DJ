#ifndef RE2DJ_INPUT_EZ2DANCER_IO_PORT_BUS_H_
#define RE2DJ_INPUT_EZ2DANCER_IO_PORT_BUS_H_

#include <array>
#include <cstdint>

#include "re2dj/input/ez2dancer_io_board.h"

namespace re2dj::input
{

// Word-wide counterpart to LegacyIoPortBus. The two are separate classes
// rather than one widened bus because they serve different boards over
// different port ranges, and five products already depend on the byte path
// behaving exactly as it does.
class Ez2DancerIoPortBus
{
public:
    Ez2DancerIoPortBus();

    bool ReadWord(std::uint16_t port, std::uint16_t* value);
    bool WriteWord(std::uint16_t port, std::uint16_t value);
    bool SetInputWord(std::uint16_t port, std::uint16_t value);
    bool ClearInputOverride(std::uint16_t port);
    bool GetLastOutputWord(std::uint16_t port, std::uint16_t* value) const;
    bool SetButton(Ez2DancerButton button, bool pressed);
    bool GetLight(Ez2DancerLight light, bool* enabled) const;

private:
    // The board's ports are even addresses across this span, so the index is
    // halved rather than used directly.
    static constexpr std::uint16_t kFirstPort = 0x300;
    static constexpr std::uint16_t kLastPort = 0x30c;
    static constexpr std::size_t kPortCount = (kLastPort - kFirstPort) / 2 + 1;

    static bool PortIndex(std::uint16_t port, std::size_t* index);

    Ez2DancerIoBoard board_;
    std::array<std::uint16_t, kPortCount> input_overrides_ = {};
    std::array<bool, kPortCount> input_overridden_ = {};
    std::array<std::uint16_t, kPortCount> outputs_ = {};
    std::array<bool, kPortCount> output_written_ = {};
};

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_EZ2DANCER_IO_PORT_BUS_H_
