#pragma once

#include <windows.h>

extern "C" __declspec(dllexport) HRESULT WINAPI
Re2djHleDirectInputCreateA(HINSTANCE hinst,
                           DWORD dwVersion,
                           void** direct_input,
                           IUnknown* punkOuter);
