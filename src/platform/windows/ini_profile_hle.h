#ifndef RE2DJ_PLATFORM_WINDOWS_INI_PROFILE_HLE_H_
#define RE2DJ_PLATFORM_WINDOWS_INI_PROFILE_HLE_H_

#define NOMINMAX
#include <windows.h>

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_demo_volume;
extern "C" __declspec(dllexport) UINT WINAPI Re2djHleGetPrivateProfileIntA(
    LPCSTR section, LPCSTR key, INT default_value, LPCSTR filename);

#endif  // RE2DJ_PLATFORM_WINDOWS_INI_PROFILE_HLE_H_
