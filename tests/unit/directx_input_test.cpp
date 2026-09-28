#include "re2dj/directx/directinput.h"

#include <array>
#include <cstdint>

#include "test_support.h"

namespace
{

namespace dx = re2dj::directx;

// The system keyboard and mouse, and the interfaces that answer as
// themselves.
void CheckDevices(re2dj::test::Context& context)
{
    RE2DJ_CHECK(context, dx::DeviceKindOf(dx::kGuidSysKeyboard) == dx::InputDeviceKind::kKeyboard);
    RE2DJ_CHECK(context, dx::DeviceKindOf(dx::kGuidSysMouse) == dx::InputDeviceKind::kMouse);
    RE2DJ_CHECK(context, !dx::DeviceKindOf(dx::kIidDirectInputA).has_value());
    RE2DJ_CHECK(context, dx::IsDirectInputInterface(dx::kIidDirectInput7A));
    RE2DJ_CHECK(context, !dx::IsDirectInputInterface(dx::kIidDirectInputDeviceA));
    RE2DJ_CHECK(context, dx::IsDirectInputDeviceInterface(dx::kIidDirectInputDevice7A));
    RE2DJ_CHECK(context, dx::IsDirectInputDeviceInterface(dx::kIidUnknown));
}

// Held keys become 0x80 at their scan codes, within the buffer; a mouse
// reports held buttons only when the buffer holds a DIMOUSESTATE.
void CheckStates(re2dj::test::Context& context)
{
    dx::InputSnapshot snapshot;
    snapshot.keys.set(0x1C);
    snapshot.keys.set(0xC8);
    snapshot.mouse_buttons[1] = true;
    std::array<std::uint8_t, 256> keyboard{};
    keyboard.fill(0x55);
    dx::ComposeDeviceState(dx::InputDeviceKind::kKeyboard, snapshot, keyboard);
    RE2DJ_CHECK_EQ(context, keyboard[0x1C], 0x80);
    RE2DJ_CHECK_EQ(context, keyboard[0xC8], 0x80);
    RE2DJ_CHECK_EQ(context, keyboard[0x1D], 0);
    std::array<std::uint8_t, 0x80> short_keyboard{};
    dx::ComposeDeviceState(dx::InputDeviceKind::kKeyboard, snapshot, short_keyboard);
    RE2DJ_CHECK_EQ(context, short_keyboard[0x1C], 0x80);

    std::array<std::uint8_t, 16> mouse{};
    dx::ComposeDeviceState(dx::InputDeviceKind::kMouse, snapshot, mouse);
    RE2DJ_CHECK_EQ(context, mouse[12], 0);
    RE2DJ_CHECK_EQ(context, mouse[13], 0x80);
    RE2DJ_CHECK_EQ(context, mouse[0], 0);
    std::array<std::uint8_t, 8> small_mouse{};
    small_mouse.fill(1);
    dx::ComposeDeviceState(dx::InputDeviceKind::kMouse, snapshot, small_mouse);
    RE2DJ_CHECK_EQ(context, small_mouse[7], 0);
}

}  // namespace

void RunDirectXInputTests(re2dj::test::Context& context)
{
    CheckDevices(context);
    CheckStates(context);
}
