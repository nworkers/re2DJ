#ifndef RE2DJ_PLATFORM_WINDOWS_EZ2DJ_KEYBOARD_INPUT_H_
#define RE2DJ_PLATFORM_WINDOWS_EZ2DJ_KEYBOARD_INPUT_H_

#include <array>
#include <cstdint>
#include <string>

#include "re2dj/input/legacy_io_port_bus.h"

namespace re2dj::platform::windows
{

class Ez2DjKeyboardInput
{
public:
    bool Initialize(const char* path, std::string* error);
    void Poll(re2dj::input::LegacyIoPortBus* bus, std::uint64_t now_ms);

private:
    static constexpr std::size_t kButtonCount =
        static_cast<std::size_t>(re2dj::input::Ez2DjButton::kCount);

    std::array<int, kButtonCount> button_keys_ = {};
    std::array<int, 4> turntable_keys_ = {};
    std::array<std::uint8_t, 2> turntable_positions_ = {0x80, 0x80};
    std::uint8_t turntable_step_ = 4;
    std::uint64_t last_turntable_update_ms_ = 0;
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_EZ2DJ_KEYBOARD_INPUT_H_
