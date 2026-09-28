#include "ini_profile_hle.h"

#include <cstring>
#include <optional>

#include "audio_volume_trace.h"
#include "re2dj/hle/private_profile.h"

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_demo_volume = re2dj::hle::kDefaultDemoVolume;

extern "C" __declspec(dllexport) UINT WINAPI Re2djHleGetPrivateProfileIntA(
    LPCSTR section, LPCSTR key, INT default_value, LPCSTR filename)
{
    const std::optional<std::uint32_t> configured =
        section == nullptr || key == nullptr
            ? std::nullopt
            : re2dj::hle::PrivateProfileIntOverride(section, key, g_re2dj_demo_volume);
    if (configured.has_value())
    {
        const UINT value = static_cast<UINT>(*configured);
        Re2djAudioTrace("ini:demo-volume configured=%u", value);
        return value;
    }
    return GetPrivateProfileIntA(section, key, default_value, filename);
}
