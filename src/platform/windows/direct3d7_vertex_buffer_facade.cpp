#define NOMINMAX
#define CINTERFACE
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include <cstring>
#include <memory>
#include <new>
#include <span>

#include "direct3d7_vertex_buffer_facade.h"
#include "graphics_trace_log.h"
#include "re2dj/graphics/legacy_vertex_buffer.h"

namespace re2dj::platform::windows
{
namespace
{

struct VertexBuffer7Facade
{
    IDirect3DVertexBuffer7 interface_value = {};
    volatile LONG references = 1;
    DWORD magic = kDirect3DVertexBufferMagic;
    DirectDrawComContext* context = nullptr;
    D3DVERTEXBUFFERDESC descriptor = {};
    std::unique_ptr<re2dj::graphics::LegacyVertexBuffer> buffer;
};

VertexBuffer7Facade* FacadeFromInterface(IDirect3DVertexBuffer7* self)
{
    return reinterpret_cast<VertexBuffer7Facade*>(
        reinterpret_cast<unsigned char*>(self) -
        offsetof(VertexBuffer7Facade, interface_value));
}

// A facade whose magic no longer matches has been released, so its storage
// must not be touched.
VertexBuffer7Facade* LiveFacadeFromInterface(IDirect3DVertexBuffer7* self)
{
    if (self == nullptr)
    {
        return nullptr;
    }
    VertexBuffer7Facade* const facade = FacadeFromInterface(self);
    return facade->magic == kDirect3DVertexBufferMagic ? facade : nullptr;
}

ULONG WINAPI Vb7AddRef(IDirect3DVertexBuffer7* self);

HRESULT WINAPI Vb7QueryInterface(IDirect3DVertexBuffer7* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    if (LiveFacadeFromInterface(self) == nullptr)
    {
        return E_FAIL;
    }
    if (!IsEqualGUID(iid, IID_IUnknown) && !IsEqualGUID(iid, IID_IDirect3DVertexBuffer7))
    {
        return E_NOINTERFACE;
    }
    *object = self;
    Vb7AddRef(self);
    return S_OK;
}

ULONG WINAPI Vb7AddRef(IDirect3DVertexBuffer7* self)
{
    VertexBuffer7Facade* const facade = LiveFacadeFromInterface(self);
    if (facade == nullptr)
    {
        return 0;
    }
    return static_cast<ULONG>(InterlockedIncrement(&facade->references));
}

ULONG WINAPI Vb7Release(IDirect3DVertexBuffer7* self)
{
    VertexBuffer7Facade* const facade = LiveFacadeFromInterface(self);
    if (facade == nullptr)
    {
        return 0;
    }
    const LONG references = InterlockedDecrement(&facade->references);
    if (references == 0)
    {
        WriteGraphicsTraceLine("re2dj:hle:IDirect3DVertexBuffer7::Release:destroyed");
        facade->magic = 0;
        delete facade;
    }
    return static_cast<ULONG>(references);
}

HRESULT WINAPI Vb7Lock(IDirect3DVertexBuffer7* self, DWORD flags, LPVOID* data, LPDWORD size)
{
    if (data == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *data = nullptr;
    if (size != nullptr)
    {
        *size = 0;
    }
    VertexBuffer7Facade* const facade = LiveFacadeFromInterface(self);
    if (facade == nullptr || facade->buffer == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    const std::span<std::byte> vertices = facade->buffer->Lock();
    if (vertices.empty())
    {
        WriteGraphicsTraceLine("re2dj:hle:IDirect3DVertexBuffer7::Lock:already-locked");
        return D3DERR_VERTEXBUFFERLOCKED;
    }
    *data = vertices.data();
    if (size != nullptr)
    {
        *size = static_cast<DWORD>(vertices.size());
    }
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirect3DVertexBuffer7::Lock flags=0x%08lx bytes=%lu size_out=%s",
        static_cast<unsigned long>(flags),
        static_cast<unsigned long>(vertices.size()),
        size == nullptr ? "none" : "present");
    return D3D_OK;
}

HRESULT WINAPI Vb7Unlock(IDirect3DVertexBuffer7* self)
{
    VertexBuffer7Facade* const facade = LiveFacadeFromInterface(self);
    if (facade == nullptr || facade->buffer == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    if (!facade->buffer->Unlock())
    {
        WriteGraphicsTraceLine("re2dj:hle:IDirect3DVertexBuffer7::Unlock:not-locked");
        return DDERR_NOTLOCKED;
    }
    WriteGraphicsTraceLine("re2dj:hle:IDirect3DVertexBuffer7::Unlock");
    return D3D_OK;
}

// The transform entry points report success without transforming anything.
// Nothing consumes transformed vertices on the DirectX 7 path yet, and
// inventing an unobserved result would be worse than reporting none.
HRESULT WINAPI Vb7ProcessVertices(IDirect3DVertexBuffer7* self,
                                  DWORD operation,
                                  DWORD destination_index,
                                  DWORD count,
                                  IDirect3DVertexBuffer7*,
                                  DWORD,
                                  IDirect3DDevice7*,
                                  DWORD)
{
    if (LiveFacadeFromInterface(self) == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirect3DVertexBuffer7::ProcessVertices op=0x%08lx destination=%lu count=%lu",
        static_cast<unsigned long>(operation),
        static_cast<unsigned long>(destination_index),
        static_cast<unsigned long>(count));
    return D3D_OK;
}

HRESULT WINAPI Vb7GetVertexBufferDesc(IDirect3DVertexBuffer7* self,
                                      LPD3DVERTEXBUFFERDESC descriptor)
{
    if (descriptor == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    VertexBuffer7Facade* const facade = LiveFacadeFromInterface(self);
    if (facade == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    const DWORD requested = descriptor->dwSize;
    if (requested < sizeof(D3DVERTEXBUFFERDESC))
    {
        return DDERR_INVALIDPARAMS;
    }
    *descriptor = facade->descriptor;
    descriptor->dwSize = requested;
    WriteGraphicsTraceLine("re2dj:hle:IDirect3DVertexBuffer7::GetVertexBufferDesc");
    return D3D_OK;
}

HRESULT WINAPI Vb7Optimize(IDirect3DVertexBuffer7* self, IDirect3DDevice7*, DWORD)
{
    if (LiveFacadeFromInterface(self) == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    WriteGraphicsTraceLine("re2dj:hle:IDirect3DVertexBuffer7::Optimize");
    return D3D_OK;
}

HRESULT WINAPI Vb7ProcessVerticesStrided(IDirect3DVertexBuffer7* self,
                                         DWORD operation,
                                         DWORD destination_index,
                                         DWORD count,
                                         LPD3DDRAWPRIMITIVESTRIDEDDATA,
                                         DWORD,
                                         IDirect3DDevice7*,
                                         DWORD)
{
    if (LiveFacadeFromInterface(self) == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirect3DVertexBuffer7::ProcessVerticesStrided op=0x%08lx destination=%lu count=%lu",
        static_cast<unsigned long>(operation),
        static_cast<unsigned long>(destination_index),
        static_cast<unsigned long>(count));
    return D3D_OK;
}

IDirect3DVertexBuffer7Vtbl* VertexBuffer7Vtable()
{
    static IDirect3DVertexBuffer7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = Vb7QueryInterface;
        table.AddRef = Vb7AddRef;
        table.Release = Vb7Release;
        table.Lock = Vb7Lock;
        table.Unlock = Vb7Unlock;
        table.ProcessVertices = Vb7ProcessVertices;
        table.GetVertexBufferDesc = Vb7GetVertexBufferDesc;
        table.Optimize = Vb7Optimize;
        table.ProcessVerticesStrided = Vb7ProcessVerticesStrided;
        initialized = true;
    }
    return &table;
}

}  // namespace

HRESULT CreateDirect3DVertexBuffer7Facade(DirectDrawComContext* context,
                                          const _D3DVERTEXBUFFERDESC* descriptor,
                                          IDirect3DVertexBuffer7** out)
{
    if (out == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *out = nullptr;
    if (descriptor == nullptr || descriptor->dwSize < sizeof(D3DVERTEXBUFFERDESC))
    {
        WriteGraphicsTraceLine(
            "re2dj:hle:IDirect3D7::CreateVertexBuffer:invalid-descriptor");
        return DDERR_INVALIDPARAMS;
    }

    re2dj::graphics::LegacyVertexBufferDesc storage_descriptor;
    storage_descriptor.size = sizeof(D3DVERTEXBUFFERDESC);
    storage_descriptor.caps = descriptor->dwCaps;
    storage_descriptor.fvf = descriptor->dwFVF;
    storage_descriptor.vertex_count = descriptor->dwNumVertices;
    std::unique_ptr<re2dj::graphics::LegacyVertexBuffer> storage =
        re2dj::graphics::LegacyVertexBuffer::Create(storage_descriptor);
    if (storage == nullptr)
    {
        WriteGraphicsTraceFormat(
            "re2dj:hle:IDirect3D7::CreateVertexBuffer:unsupported fvf=0x%08lx vertices=%lu",
            static_cast<unsigned long>(descriptor->dwFVF),
            static_cast<unsigned long>(descriptor->dwNumVertices));
        return DDERR_INVALIDPARAMS;
    }

    auto* const facade = new (std::nothrow) VertexBuffer7Facade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->interface_value.lpVtbl = VertexBuffer7Vtable();
    facade->context = context;
    facade->descriptor = *descriptor;
    facade->descriptor.dwSize = sizeof(D3DVERTEXBUFFERDESC);
    facade->buffer = std::move(storage);
    *out = &facade->interface_value;
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirect3D7::CreateVertexBuffer caps=0x%08lx fvf=0x%08lx vertices=%lu "
        "stride=%lu bytes=%lu",
        static_cast<unsigned long>(descriptor->dwCaps),
        static_cast<unsigned long>(descriptor->dwFVF),
        static_cast<unsigned long>(descriptor->dwNumVertices),
        static_cast<unsigned long>(facade->buffer->stride()),
        static_cast<unsigned long>(facade->buffer->vertices().size()));
    return D3D_OK;
}

}  // namespace re2dj::platform::windows
