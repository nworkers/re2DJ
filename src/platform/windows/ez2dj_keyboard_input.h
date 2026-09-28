#ifndef RE2DJ_PLATFORM_WINDOWS_EZ2DJ_KEYBOARD_INPUT_H_
#define RE2DJ_PLATFORM_WINDOWS_EZ2DJ_KEYBOARD_INPUT_H_

#include <array>
#include <cstdint>
#include <string>

#include "re2dj/input/ez2dj_keyboard_map.h"
#include "re2dj/input/legacy_io_port_bus.h"

namespace re2dj::platform::windows
{

class Ez2DjKeyboardInput
{
public:
    // Binds every input to its built-in default, then, when `path` names a
    // configuration file, lets that file's entries override the ones it lists.
    // A null or empty path leaves the defaults in place.
    bool Initialize(const char* path, std::string* error);
    void Poll(re2dj::input::LegacyIoPortBus* bus, std::uint64_t now_ms);

    // Bindings as they ended up, for tests.
    int button_key(re2dj::input::Ez2DjButton button) const
    {
        return button_keys_[static_cast<std::size_t>(button)];
    }
    int turntable_key(std::size_t index) const { return turntable_keys_[index]; }
    std::uint8_t turntable_step() const { return turntable_step_; }

private:
    static constexpr std::size_t kButtonCount =
        static_cast<std::size_t>(re2dj::input::Ez2DjButton::kCount);

    std::array<int, kButtonCount> button_keys_ = {};
    std::array<int, 4> turntable_keys_ = {};
    std::uint8_t turntable_step_ = re2dj::input::kEz2DjDefaultTurntableStep;
    re2dj::input::Ez2DjTurntables turntables_;
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_EZ2DJ_KEYBOARD_INPUT_H_
