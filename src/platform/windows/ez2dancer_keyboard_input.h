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
    // Binds every input to its built-in default, then, when `path` names a
    // configuration file, lets that file's entries override the ones it lists.
    // A null or empty path leaves the defaults in place.
    bool Initialize(const char* path, std::string* error);
    void Poll(re2dj::input::Ez2DancerIoPortBus* bus);

    // Bindings as they ended up, for tests.
    int button_key(re2dj::input::Ez2DancerButton button) const
    {
        return button_keys_[static_cast<std::size_t>(button)];
    }

private:
    static constexpr std::size_t kButtonCount =
        static_cast<std::size_t>(re2dj::input::Ez2DancerButton::kCount);

    std::array<int, kButtonCount> button_keys_ = {};
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_EZ2DANCER_KEYBOARD_INPUT_H_
