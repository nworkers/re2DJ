#include "re2dj/input/io_bindings.h"

#include <optional>

#include "re2dj/hle/private_profile.h"
#include "re2dj/input/ez2dancer_keyboard_map.h"
#include "re2dj/input/gamepad.h"
#include "re2dj/input/virtual_keys.h"

namespace re2dj::input
{
namespace
{

// The entry's value in the INI as GetPrivateProfileStringA would hand it
// over, or nothing when the file omits it.
std::optional<std::string> IniValue(std::string_view ini_text, std::string_view section, std::string_view name)
{
    if (ini_text.empty())
    {
        return std::nullopt;
    }
    const std::optional<std::string> value = hle::FindPrivateProfileValue(ini_text, section, name);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return hle::PrivateProfileStringValue(*value);
}

// Reads one key entry over its default; false, with error, for a name the
// key parser does not know.
bool ApplyKey(std::string_view ini_text, std::string_view section, std::string_view name, int* key, std::string* error)
{
    const std::optional<std::string> value = IniValue(ini_text, section, name);
    if (!value.has_value())
    {
        return true;
    }
    const int parsed = ParseKeyName(*value);
    if (parsed < 0)
    {
        *error = "unknown key name for " + std::string(section) + "." + std::string(name) + ": " + *value;
        return false;
    }
    *key = parsed;
    return true;
}

// Reads one [gamepad] entry over its default; false, with error, for a name
// the control parser does not know.
bool ApplyGamepad(std::string_view ini_text, std::string_view name, int* control, std::string* error)
{
    const std::optional<std::string> value = IniValue(ini_text, "gamepad", name);
    if (!value.has_value())
    {
        return true;
    }
    const int parsed = ParseGamepadControlName(*value);
    if (parsed < 0)
    {
        *error = "unknown gamepad control for gamepad." + std::string(name) + ": " + *value;
        return false;
    }
    *control = parsed;
    return true;
}

}  // namespace

Ez2DjIoBindings DefaultEz2DjIoBindings()
{
    Ez2DjIoBindings bindings;
    for (const Ez2DjButtonBinding& binding : Ez2DjButtonBindings())
    {
        const auto index = static_cast<std::size_t>(binding.button);
        bindings.button_keys[index] = ParseKeyName(binding.default_key);
        bindings.button_gamepad[index] = ParseGamepadControlName(binding.default_gamepad);
    }
    for (std::size_t index = 0; index < bindings.turntable_keys.size(); ++index)
    {
        const Ez2DjTurntableBinding& binding = Ez2DjTurntableBindings()[index];
        bindings.turntable_keys[index] = ParseKeyName(binding.default_key);
        bindings.turntable_gamepad[index] = ParseGamepadControlName(binding.default_gamepad);
    }
    bindings.turntable_step = kEz2DjDefaultTurntableStep;
    return bindings;
}

Ez2DancerIoBindings DefaultEz2DancerIoBindings()
{
    Ez2DancerIoBindings bindings;
    for (const Ez2DancerButtonBinding& binding : Ez2DancerButtonBindings())
    {
        const auto index = static_cast<std::size_t>(binding.button);
        bindings.button_keys[index] = ParseKeyName(binding.default_key);
        bindings.button_gamepad[index] = ParseGamepadControlName(binding.default_gamepad);
    }
    return bindings;
}

IoBindings DefaultIoBindings()
{
    return {DefaultEz2DjIoBindings(), DefaultEz2DancerIoBindings()};
}

bool LoadEz2DjIoBindings(std::string_view ini_text, Ez2DjIoBindings* bindings, std::string* error)
{
    if (bindings == nullptr || error == nullptr)
    {
        return false;
    }
    Ez2DjIoBindings loaded = DefaultEz2DjIoBindings();
    for (const Ez2DjButtonBinding& binding : Ez2DjButtonBindings())
    {
        const auto index = static_cast<std::size_t>(binding.button);
        if (!ApplyKey(ini_text, "buttons", binding.name, &loaded.button_keys[index], error) ||
            !ApplyGamepad(ini_text, binding.name, &loaded.button_gamepad[index], error))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < loaded.turntable_keys.size(); ++index)
    {
        const Ez2DjTurntableBinding& binding = Ez2DjTurntableBindings()[index];
        if (!ApplyKey(ini_text, "turntables", binding.name, &loaded.turntable_keys[index], error) ||
            !ApplyGamepad(ini_text, binding.name, &loaded.turntable_gamepad[index], error))
        {
            return false;
        }
    }
    // The step as GetPrivateProfileIntA reads it: an empty value is the
    // default, text without digits is 0 and so out of range.
    const std::optional<std::string> step = IniValue(ini_text, "turntables", "step");
    if (step.has_value())
    {
        const std::uint32_t value = hle::ParsePrivateProfileInt(*step, kEz2DjDefaultTurntableStep);
        if (value < 1 || value > 32)
        {
            *error = "turntables.step must be between 1 and 32";
            return false;
        }
        loaded.turntable_step = static_cast<std::uint8_t>(value);
    }
    *bindings = loaded;
    error->clear();
    return true;
}

bool LoadEz2DancerIoBindings(std::string_view ini_text, Ez2DancerIoBindings* bindings, std::string* error)
{
    if (bindings == nullptr || error == nullptr)
    {
        return false;
    }
    Ez2DancerIoBindings loaded = DefaultEz2DancerIoBindings();
    for (const Ez2DancerButtonBinding& binding : Ez2DancerButtonBindings())
    {
        const auto index = static_cast<std::size_t>(binding.button);
        if (!ApplyKey(ini_text, "buttons", binding.name, &loaded.button_keys[index], error) ||
            !ApplyGamepad(ini_text, binding.name, &loaded.button_gamepad[index], error))
        {
            return false;
        }
    }
    *bindings = loaded;
    error->clear();
    return true;
}

}  // namespace re2dj::input
