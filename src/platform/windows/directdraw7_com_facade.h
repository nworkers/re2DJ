#pragma once

#include <windows.h>

extern "C" __declspec(dllexport) HRESULT WINAPI
Re2djHleDirectDrawCreateEx(GUID* driver_guid,
                          void** direct_draw,
                          REFIID iid,
                          IUnknown* outer);
