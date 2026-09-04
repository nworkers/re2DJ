#pragma once

#include <windows.h>

#include "directdraw_com_context.h"

struct IDirect3DVertexBuffer7;
struct _D3DVERTEXBUFFERDESC;

namespace re2dj::platform::windows
{

// Creates an IDirect3DVertexBuffer7 backed by the platform-neutral vertex
// storage. The guest locks the buffer immediately after creating it and does
// not check the result, so this either yields a usable interface or reports a
// real failure; it never reports success with a null interface.
HRESULT CreateDirect3DVertexBuffer7Facade(DirectDrawComContext* context,
                                          const _D3DVERTEXBUFFERDESC* descriptor,
                                          IDirect3DVertexBuffer7** out);

}  // namespace re2dj::platform::windows
