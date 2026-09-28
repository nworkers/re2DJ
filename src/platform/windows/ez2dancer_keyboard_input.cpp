#include "ez2dancer_keyboard_input.h"

#include "keyboard_input_common.h"

#include <string>

#include "re2dj/input/ez2dancer_keyboard_map.h"

namespace re2dj::platform::windows
{

bool Ez2DancerKeyboardInput::Initialize(const char* path, std::string* error)
{
    if (error == nullptr)
    {
        return false;
    }
    const bool has_file = path != nullptr && path[0] != '\0';
    // The bindings and their defaults are shared with the Linux host.
    for (const re2dj::input::Ez2DancerButtonBinding& binding : re2dj::input::Ez2DancerButtonBindings())
    {
        const std::string default_key(binding.default_key);
        const std::string name(binding.name);
        int key = 0;
        if (!ParseKeyboardKeyName(default_key.c_str(), &key))
        {
            *error = "built-in default is not a key name: " + default_key;
            return false;
        }
        bool present = false;
        if (has_file &&
            !ReadKeyboardKeyBinding(path, "buttons", name.c_str(), &key, &present, error))
        {
            return false;
        }
        button_keys_[static_cast<std::size_t>(binding.button)] = key;
    }
    error->clear();
    return true;
}

void Ez2DancerKeyboardInput::Poll(re2dj::input::Ez2DancerIoPortBus* bus)
{
    if (bus == nullptr)
    {
        return;
    }
    for (std::size_t index = 0; index < button_keys_.size(); ++index)
    {
        bus->SetButton(static_cast<re2dj::input::Ez2DancerButton>(index),
                       IsKeyboardKeyPressed(button_keys_[index]));
    }
}

}  // namespace re2dj::platform::windows
