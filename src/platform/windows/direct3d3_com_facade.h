#ifndef RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_COM_FACADE_H_
#define RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_COM_FACADE_H_

#define NOMINMAX
#include <windows.h>
#include <ddraw.h>

extern "C" __declspec(dllexport) HRESULT WINAPI Re2djHleDirectDrawCreate(
    GUID* device_guid,
    LPDIRECTDRAW* direct_draw,
    IUnknown* outer);

#endif  // RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_COM_FACADE_H_
