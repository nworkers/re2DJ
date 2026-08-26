#ifndef RE2DJ_PLATFORM_WINDOWS_DIRECTSOUND_COM_FACADE_H_
#define RE2DJ_PLATFORM_WINDOWS_DIRECTSOUND_COM_FACADE_H_

#define DIRECTSOUND_VERSION 0x0300
#include <dsound.h>

extern "C" __declspec(dllexport) HRESULT WINAPI Re2djHleDirectSoundCreate(
    GUID* device_guid, LPDIRECTSOUND* direct_sound, IUnknown* outer);

#endif  // RE2DJ_PLATFORM_WINDOWS_DIRECTSOUND_COM_FACADE_H_
