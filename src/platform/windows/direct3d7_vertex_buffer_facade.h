#pragma once

#include <windows.h>

#include "directdraw_legacy_interop.h"

namespace re2dj::platform::windows
{

// The IDirect3DVertexBuffer7 table, typed as the DirectX 6 table it extends so
// the shared facade can install it on the vertex buffers it creates without
// naming DirectX 7 types. The object is the same object either way.
const IDirect3DVertexBufferVtbl* Direct3DVertexBuffer7VtableAsLegacy();

}  // namespace re2dj::platform::windows
