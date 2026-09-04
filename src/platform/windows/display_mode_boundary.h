#pragma once

#include <windows.h>

// The host display mode boundary.
//
// re2DJ never changes the host's display mode. The original asks for one -
// 1st SE requests 640x480x16 right after creating its window - and a mode left
// changed by an abnormal exit does not come back on its own, so the request is
// absorbed here and answered as though it had been carried out. What the guest
// sees instead is decided entirely by the window policy: a scaled window, or a
// borderless window covering the current monitor at the current resolution.
//
// This is a host-integrity guarantee rather than a per-profile capability, so
// it is installed on every run that injects the runtime and has no option to
// turn it off.

extern "C" __declspec(dllexport) LONG WINAPI Re2djHleChangeDisplaySettingsExA(
    LPCSTR device_name,
    DEVMODEA* dev_mode,
    HWND window,
    DWORD flags,
    LPVOID reserved);

extern "C" __declspec(dllexport) LONG WINAPI Re2djHleChangeDisplaySettingsA(
    DEVMODEA* dev_mode,
    DWORD flags);
