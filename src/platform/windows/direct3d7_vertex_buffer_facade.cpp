#define NOMINMAX
#define CINTERFACE
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "direct3d7_vertex_buffer_facade.h"
#include "directdraw_legacy_interop.h"
#include "graphics_trace_log.h"

// IDirect3DVertexBuffer7 repeats IDirect3DVertexBuffer slot for slot and adds
// ProcessVerticesStrided after it, so this file is a table of adoptions over
// the same object the shared facade creates. See directdraw_legacy_interop.h.

namespace re2dj::platform::windows
{
namespace
{

constexpr long kUnimplementedCallBudget = 4;

template <typename Slot, typename Implementation>
void Adopt(Slot& slot, Implementation implementation)
{
    slot = reinterpret_cast<Slot>(implementation);
}

HRESULT WINAPI Vb7QueryInterface(IDirect3DVertexBuffer7* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IDirect3DVertexBuffer7) ||
        IsEqualGUID(iid, IID_IDirect3DVertexBuffer))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

HRESULT WINAPI Vb7ProcessVerticesStrided(IDirect3DVertexBuffer7*,
                                         DWORD,
                                         DWORD,
                                         DWORD,
                                         D3DDRAWPRIMITIVESTRIDEDDATA*,
                                         DWORD,
                                         IDirect3DDevice7*,
                                         DWORD)
{
    static GraphicsCallLedger ledger = {"ProcessVerticesStrided", kUnimplementedCallBudget};
    ReportUnimplementedGraphicsCall("IDirect3DVertexBuffer7", &ledger);
    return DDERR_UNSUPPORTED;
}

IDirect3DVertexBuffer7Vtbl* VertexBuffer7Vtable()
{
    static IDirect3DVertexBuffer7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        const IDirect3DVertexBufferVtbl* const legacy = LegacyDirect3DVertexBufferVtable();

        // Adopted: the DirectX 6 implementation, unchanged. Only the static
        // types of the device and buffer parameters differ.
        Adopt(table.AddRef, legacy->AddRef);
        Adopt(table.Release, legacy->Release);
        Adopt(table.Lock, legacy->Lock);
        Adopt(table.Unlock, legacy->Unlock);
        Adopt(table.ProcessVertices, legacy->ProcessVertices);
        Adopt(table.GetVertexBufferDesc, legacy->GetVertexBufferDesc);
        Adopt(table.Optimize, legacy->Optimize);

        table.QueryInterface = Vb7QueryInterface;
        table.ProcessVerticesStrided = Vb7ProcessVerticesStrided;
        initialized = true;
    }
    return &table;
}

}  // namespace

const IDirect3DVertexBufferVtbl* Direct3DVertexBuffer7VtableAsLegacy()
{
    return reinterpret_cast<const IDirect3DVertexBufferVtbl*>(VertexBuffer7Vtable());
}

}  // namespace re2dj::platform::windows
