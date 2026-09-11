#ifndef RE2DJ_PLATFORM_WINDOWS_EZ2DANCER_KEYBOARD_INPUT_H_
#define RE2DJ_PLATFORM_WINDOWS_EZ2DANCER_KEYBOARD_INPUT_H_

#include <array>
#include <string>

#include "re2dj/input/ez2dancer_io_port_bus.h"

namespace re2dj::platform::windows
{

class Ez2DancerKeyboardInput
{
public:
    bool Initialize(const char* path, std::string* error);
    void Poll(re2dj::input::Ez2DancerIoPortBus* bus);

private:
    static constexpr std::size_t kButtonCount =
        static_cast<std::size_t>(re2dj::input::Ez2DancerButton::kCount);

    std::array<int, kButtonCount> button_keys_ = {};
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_EZ2DANCER_KEYBOARD_INPUT_H_
