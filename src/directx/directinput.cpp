#include "re2dj/directx/directinput.h"

#include <algorithm>
#include <cstddef>

namespace re2dj::directx
{

bool IsDirectInputInterface(const Guid& iid)
{
    return iid == kIidUnknown || iid == kIidDirectInputA || iid == kIidDirectInput7A;
}

bool IsDirectInputDeviceInterface(const Guid& iid)
{
    return iid == kIidUnknown || iid == kIidDirectInputDeviceA || iid == kIidDirectInputDevice7A;
}

std::optional<InputDeviceKind> DeviceKindOf(const Guid& instance)
{
    if (instance == kGuidSysKeyboard)
    {
        return InputDeviceKind::kKeyboard;
    }
    if (instance == kGuidSysMouse)
    {
        return InputDeviceKind::kMouse;
    }
    return std::nullopt;
}

void ComposeDeviceState(InputDeviceKind kind, const InputSnapshot& snapshot, std::span<std::uint8_t> state)
{
    std::fill(state.begin(), state.end(), std::uint8_t{0});
    if (kind == InputDeviceKind::kKeyboard)
    {
        const std::size_t keys = std::min(state.size(), snapshot.keys.size());
        for (std::size_t key = 1; key < keys; ++key)
        {
            if (snapshot.keys.test(key))
            {
                state[key] = 0x80;
            }
        }
        return;
    }
    if (state.size() < sizeof(DiMouseState))
    {
        return;
    }
    constexpr std::size_t kButtons = offsetof(DiMouseState, buttons);
    for (std::size_t button = 0; button < snapshot.mouse_buttons.size(); ++button)
    {
        if (snapshot.mouse_buttons[button])
        {
            state[kButtons + button] = 0x80;
        }
    }
}

}  // namespace re2dj::directx
