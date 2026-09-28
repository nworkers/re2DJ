#define NOMINMAX
#include <windows.h>

#include "ez2dj_keyboard_input.h"
#include "keyboard_input_common.h"

#include <array>
#include <string>

namespace re2dj::platform::windows
{

bool Ez2DjKeyboardInput::Initialize(const char* path, std::string* error)
{
    if (error == nullptr)
    {
        return false;
    }
    const bool has_file = path != nullptr && path[0] != '\0';
    for (const re2dj::input::Ez2DjButtonBinding& binding : re2dj::input::Ez2DjButtonBindings())
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
    for (std::size_t index = 0; index < turntable_keys_.size(); ++index)
    {
        const re2dj::input::Ez2DjTurntableBinding& binding = re2dj::input::Ez2DjTurntableBindings()[index];
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
            !ReadKeyboardKeyBinding(path, "turntables", name.c_str(), &key, &present, error))
        {
            return false;
        }
        turntable_keys_[index] = key;
    }
    UINT step = re2dj::input::kEz2DjDefaultTurntableStep;
    if (has_file)
    {
        step = GetPrivateProfileIntA("turntables", "step", re2dj::input::kEz2DjDefaultTurntableStep, path);
    }
    if (step < 1 || step > 32)
    {
        *error = "turntables.step must be between 1 and 32";
        return false;
    }
    turntable_step_ = static_cast<std::uint8_t>(step);
    error->clear();
    return true;
}

void Ez2DjKeyboardInput::Poll(re2dj::input::LegacyIoPortBus* bus, std::uint64_t now_ms)
{
    if (bus == nullptr) return;
    for (std::size_t index = 0; index < button_keys_.size(); ++index)
    {
        bus->SetButton(static_cast<re2dj::input::Ez2DjButton>(index),
                       IsKeyboardKeyPressed(button_keys_[index]));
    }
    std::array<bool, 4> held = {};
    for (std::size_t index = 0; index < held.size(); ++index)
    {
        held[index] = IsKeyboardKeyPressed(turntable_keys_[index]);
    }
    turntables_.Update(bus, now_ms, held, turntable_step_);
}

}  // namespace re2dj::platform::windows
