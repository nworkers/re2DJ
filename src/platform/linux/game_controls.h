#ifndef RE2DJ_PLATFORM_LINUX_GAME_CONTROLS_H_
#define RE2DJ_PLATFORM_LINUX_GAME_CONTROLS_H_

#include <cstdint>

namespace re2dj::ui
{
class Osd;
}

namespace re2dj::platform::linux
{

// The guest state the Linux OSD may switch, as the Windows runtime's
// game_controls does. The run arms an address only for the exact build the
// profile confirmed it in (target::ArmedAutoplayFlagRva).
//
// The guest's 32-bit flag that makes it hit notes by itself, at the guest's
// own address; zero disarms it.
void ArmAutoplayFlag(std::uint32_t guest_address);
// The controls offered for this run: Autoplay when an address is armed.
void AddGameControls(ui::Osd* osd);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_GAME_CONTROLS_H_
