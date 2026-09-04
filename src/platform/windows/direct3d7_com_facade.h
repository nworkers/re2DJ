#pragma once

#include <windows.h>
#include "directdraw_com_context.h"

namespace re2dj::platform::windows
{

// Creates or queries an IDirect3D7 interface bound to the shared DirectDraw context.
HRESULT CreateDirect3D7Facade(DirectDrawComContext* context, void** object);

}  // namespace re2dj::platform::windows
