#include "game_controls.h"

#include <cstdint>

#include "re2dj/ui/osd.h"

namespace re2dj::platform::native
{
namespace
{

std::uint32_t g_autoplay_flag_address = 0;

// The guest runs at its own addresses inside this process, and the OSD reads
// and writes only from the present path, which holds the guest lock.
std::uint32_t* AutoplayFlag()
{
    return reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(g_autoplay_flag_address));
}

bool ReadAutoplay(void*, bool* value)
{
    if (g_autoplay_flag_address == 0)
    {
        return false;
    }
    *value = *AutoplayFlag() != 0;
    return true;
}

bool WriteAutoplay(void*, bool value)
{
    if (g_autoplay_flag_address == 0)
    {
        return false;
    }
    *AutoplayFlag() = value ? 1u : 0u;
    return true;
}

}  // namespace

void ArmAutoplayFlag(std::uint32_t guest_address)
{
    g_autoplay_flag_address = guest_address;
}

void AddGameControls(ui::Osd* osd)
{
    if (osd == nullptr || g_autoplay_flag_address == 0)
    {
        return;
    }
    ui::OsdToggle autoplay;
    autoplay.label = "Autoplay";
    autoplay.read = &ReadAutoplay;
    autoplay.write = &WriteAutoplay;
    osd->AddToggle(autoplay);
}

}  // namespace re2dj::platform::native
