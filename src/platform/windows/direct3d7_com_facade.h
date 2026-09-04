#pragma once

#include <windows.h>

#include "directdraw_legacy_interop.h"

namespace re2dj::platform::windows
{

// Creates the IDirect3D7 interface for a root the shared facade created.
//
// IDirect3D7 reorders its predecessor rather than extending it, so unlike the
// DirectDraw interfaces it cannot be a second table on the same object: it is a
// small object of its own that holds a reference to the root and forwards to
// the DirectX 6 Direct3D implementation.
HRESULT CreateDirect3D7Facade(IDirectDraw4* root, void** object);

// The DirectX 7 device table, typed as the DirectX 6 table so the shared facade
// can install it on the devices it creates without naming DirectX 7 types.
const IDirect3DDevice3Vtbl* Direct3DDevice7VtableAsLegacy();

}  // namespace re2dj::platform::windows
