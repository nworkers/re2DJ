#include "ini_profile_hle.h"

#include <cstring>

#include "audio_volume_trace.h"

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_demo_volume = 3;

extern "C" __declspec(dllexport) UINT WINAPI Re2djHleGetPrivateProfileIntA(
    LPCSTR section, LPCSTR key, INT default_value, LPCSTR filename)
{
    if (section != nullptr && key != nullptr &&
        _stricmp(section, "GAMEASSIGNMENTS") == 0 &&
        _stricmp(key, "DemoVolume") == 0 && g_re2dj_demo_volume <= 3)
    {
        const UINT value = static_cast<UINT>(g_re2dj_demo_volume);
        Re2djAudioTrace("ini:demo-volume configured=%u", value);
        return value;
    }
    return GetPrivateProfileIntA(section, key, default_value, filename);
}
