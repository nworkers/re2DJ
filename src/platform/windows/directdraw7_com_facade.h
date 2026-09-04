#pragma once

#include <windows.h>

// The guest-facing entry point. Everything behind it - the DirectX 7 vtables
// and the DirectX 6 objects they are installed on - stays inside the facade
// sources, which compile with CINTERFACE so the vtable structures exist. This
// header is included by the injected runtime, which does not, so it declares
// only the entry point.
extern "C" __declspec(dllexport) HRESULT WINAPI
Re2djHleDirectDrawCreateEx(GUID* driver_guid,
                           void** direct_draw,
                           REFIID iid,
                           IUnknown* outer);
