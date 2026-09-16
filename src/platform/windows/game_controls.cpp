#include "game_controls.h"

#include <cstdint>
#include <string>
#include <vector>

extern "C" __declspec(dllexport) unsigned long g_re2dj_autoplay_flag_address = 0;
extern "C" __declspec(dllexport) char g_re2dj_target_id[32] = {};
extern "C" __declspec(dllexport) char g_re2dj_executable_name[64] = {};

namespace re2dj::platform::windows
{
namespace
{

// The flag lives in the guest's own data section, in this process. The launcher
// arms the address only after matching the executable's build, so it is read
// and written directly.
bool ReadAutoplay(void*, bool* value)
{
    const auto* const flag =
        reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(g_re2dj_autoplay_flag_address));
    if (flag == nullptr)
    {
        return false;
    }
    *value = *flag != 0;
    return true;
}

bool WriteAutoplay(void*, bool value)
{
    auto* const flag =
        reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(g_re2dj_autoplay_flag_address));
    if (flag == nullptr)
    {
        return false;
    }
    *flag = value ? 1u : 0u;
    return true;
}

}  // namespace

void RegisterGameControls(re2dj::ui::Osd* osd)
{
    if (osd == nullptr)
    {
        return;
    }
    g_re2dj_target_id[sizeof(g_re2dj_target_id) - 1] = '\0';
    g_re2dj_executable_name[sizeof(g_re2dj_executable_name) - 1] = '\0';
    // Version and build date match what the window title shows.
    std::vector<std::string> lines;
    lines.push_back(std::string("re2DJ v") + RE2DJ_VERSION + " " + __DATE__);
    lines.push_back(std::string("Target Profile : ") + g_re2dj_target_id);
    lines.push_back(std::string("Executable : ") + g_re2dj_executable_name);
    osd->SetInfoLines(lines);
    // Offered only when the launcher armed it, which it does only for the exact
    // build the address was confirmed in.
    if (g_re2dj_autoplay_flag_address != 0)
    {
        re2dj::ui::OsdToggle autoplay;
        autoplay.label = "Autoplay";
        autoplay.read = &ReadAutoplay;
        autoplay.write = &WriteAutoplay;
        osd->AddToggle(autoplay);
    }
}

}  // namespace re2dj::platform::windows
