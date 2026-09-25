#define NOMINMAX
#define CINTERFACE
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include <cstdio>
#include <cstring>
#include <new>

#include "direct3d7_com_facade.h"
#include "directdraw_legacy_interop.h"
#include "directx_abi_windows.h"
#include "graphics_trace_log.h"
#include "re2dj/directx/direct3d_description.h"

// IDirect3D7 and IDirect3DDevice7 reorder their predecessors rather than
// extending them, so unlike the DirectDraw interfaces they cannot be adopted
// slot for slot. What carries over is the behavior: a DirectX 7 method whose
// meaning did not change holds the DirectX 6 function, and the device object
// behind the interface is the same object the DirectX 6 facade creates, with a
// DirectX 7 table installed on it.
//
// IDirect3D7 is the one exception. Its predecessor lives on the root object as
// IDirect3D3, and a second Direct3D table cannot be installed there, so this
// interface is a small object of its own that holds the root.
//
// See directdraw_legacy_interop.h for the layering this rests on.

namespace re2dj::platform::windows
{
namespace
{

// The number of calls one unimplemented slot records before going quiet.
constexpr long kUnimplementedCallBudget = 4;

struct Direct3D7Facade
{
    IDirect3D7 interface_value;
    volatile LONG references = 1;
    IDirectDraw4* root = nullptr;
};

// Forward declarations
IDirect3D7Vtbl* D3D7Vtable();
IDirect3DDevice7Vtbl* Device7Vtable();

// Installs a DirectX 6 implementation into a DirectX 7 vtable slot. The two
// declarations differ only in the static types of pointer parameters, which are
// the same width and are passed the same way, so one function serves both.
template <typename Slot, typename Implementation>
void Adopt(Slot& slot, Implementation implementation)
{
    slot = reinterpret_cast<Slot>(implementation);
}

// The DirectX 6 device reached through its own interface type. The object
// behind an IDirect3DDevice7 this facade hands out is the DirectX 6 device
// object, so the cast is the same pointer.
IDirect3DDevice3* LegacyDevice(IDirect3DDevice7* self)
{
    return reinterpret_cast<IDirect3DDevice3*>(self);
}

IDirectDrawSurface4* LegacySurface(IDirectDrawSurface7* surface)
{
    return reinterpret_cast<IDirectDrawSurface4*>(surface);
}

HRESULT WINAPI D3d7QueryInterface(IDirect3D7* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IDirect3D7))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG WINAPI D3d7AddRef(IDirect3D7* self)
{
    auto* const facade = reinterpret_cast<Direct3D7Facade*>(self);
    return static_cast<ULONG>(InterlockedIncrement(&facade->references));
}

ULONG WINAPI D3d7Release(IDirect3D7* self)
{
    auto* const facade = reinterpret_cast<Direct3D7Facade*>(self);
    const LONG count = InterlockedDecrement(&facade->references);
    if (count <= 0)
    {
        IDirectDraw4* const root = facade->root;
        delete facade;
        // The root reference this object took in CreateDirect3D7Facade is
        // released after the object itself, so the root cannot be destroyed
        // while the object is still being torn down.
        if (root != nullptr)
        {
            root->lpVtbl->Release(root);
        }
        return 0;
    }
    return static_cast<ULONG>(count);
}

HRESULT WINAPI D3d7EnumDevices(IDirect3D7* self, LPD3DENUMDEVICESCALLBACK7 callback, void* arg)
{
    (void)self;
    WriteGraphicsTraceFormat("re2dj:hle:IDirect3D7::EnumDevices callback=%p", callback);
    if (callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    for (const re2dj::directx::Direct3DDevice& device : re2dj::directx::Direct3D7Devices())
    {
        D3DDEVICEDESC7 desc = {};
        CopyFromCore(&desc, re2dj::directx::DeviceDescription(device.guid, device.hardware_transform_and_light));
        char description[128] = {};
        char name[64] = {};
        std::snprintf(description, sizeof(description), "%.*s", static_cast<int>(device.description.size()),
                      device.description.data());
        std::snprintf(name, sizeof(name), "%.*s", static_cast<int>(device.name.size()), device.name.data());
        const HRESULT callback_result = callback(description, name, &desc, arg);
        WriteGraphicsTraceFormat(
            "re2dj:hle:IDirect3D7::EnumDevices:device name=%s guid={%08lx-%04x-%04x} "
            "devcaps=0x%08lx render-depths=0x%08lx z-depths=0x%08lx texop=0x%08lx "
            "tri-texture=0x%08lx stages=%u callback_ret=0x%08lx",
            name,
            desc.deviceGUID.Data1,
            desc.deviceGUID.Data2,
            desc.deviceGUID.Data3,
            static_cast<unsigned long>(desc.dwDevCaps),
            static_cast<unsigned long>(desc.dwDeviceRenderBitDepth),
            static_cast<unsigned long>(desc.dwDeviceZBufferBitDepth),
            static_cast<unsigned long>(desc.dwTextureOpCaps),
            static_cast<unsigned long>(desc.dpcTriCaps.dwTextureCaps),
            static_cast<unsigned>(desc.wMaxTextureBlendStages),
            static_cast<unsigned long>(callback_result));
        if (callback_result == D3DENUMRET_CANCEL)
        {
            break;
        }
    }
    return D3D_OK;
}

// DirectX 7 dropped the aggregation parameter its predecessor carried. Nothing
// else changed, so the call goes straight through to the shared implementation,
// which installs this facade's device table on the object it creates.
HRESULT WINAPI D3d7CreateDevice(IDirect3D7* self,
                                REFCLSID rclsid,
                                IDirectDrawSurface7* surface,
                                IDirect3DDevice7** device)
{
    WriteGraphicsTraceLine("re2dj:hle:IDirect3D7::CreateDevice");
    if (device == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *device = nullptr;
    auto* const facade = reinterpret_cast<Direct3D7Facade*>(self);
    IDirect3D3* const legacy = LegacyDirect3DOfRoot(facade->root);
    if (legacy == nullptr || legacy->lpVtbl->CreateDevice == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    IDirect3DDevice3* created = nullptr;
    const HRESULT result = legacy->lpVtbl->CreateDevice(
        legacy, rclsid, LegacySurface(surface), &created, nullptr);
    if (result != D3D_OK)
    {
        WriteGraphicsTraceFormat("re2dj:hle:IDirect3D7::CreateDevice:failed=0x%08lx",
                                 static_cast<unsigned long>(result));
        return result;
    }
    *device = reinterpret_cast<IDirect3DDevice7*>(created);
    return D3D_OK;
}

// Same shape of change: the aggregation parameter is gone and nothing else is.
HRESULT WINAPI D3d7CreateVertexBuffer(IDirect3D7* self,
                                      D3DVERTEXBUFFERDESC* desc,
                                      IDirect3DVertexBuffer7** vb,
                                      DWORD flags)
{
    if (vb == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *vb = nullptr;
    auto* const facade = reinterpret_cast<Direct3D7Facade*>(self);
    IDirect3D3* const legacy = LegacyDirect3DOfRoot(facade->root);
    if (legacy == nullptr || legacy->lpVtbl->CreateVertexBuffer == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    IDirect3DVertexBuffer* created = nullptr;
    const HRESULT result =
        legacy->lpVtbl->CreateVertexBuffer(legacy, desc, &created, flags, nullptr);
    if (result != D3D_OK)
    {
        return result;
    }
    *vb = reinterpret_cast<IDirect3DVertexBuffer7*>(created);
    return D3D_OK;
}

HRESULT WINAPI D3d7EnumZBufferFormats(IDirect3D7* self,
                                     REFCLSID rclsid,
                                     LPD3DENUMPIXELFORMATSCALLBACK callback,
                                     void* arg)
{
    (void)self;
    (void)rclsid;
    WriteGraphicsTraceLine("re2dj:hle:IDirect3D7::EnumZBufferFormats");
    if (callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DDPIXELFORMAT ddpf = {};
    CopyFromCore(&ddpf, re2dj::directx::Depth16Format());
    callback(&ddpf, arg);
    return D3D_OK;
}

HRESULT WINAPI D3d7EvictManagedTextures(IDirect3D7* self)
{
    (void)self;
    WriteGraphicsTraceLine("re2dj:hle:IDirect3D7::EvictManagedTextures");
    return D3D_OK;
}

IDirect3D7Vtbl* D3D7Vtable()
{
    static IDirect3D7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = D3d7QueryInterface;
        table.AddRef = D3d7AddRef;
        table.Release = D3d7Release;
        table.EnumDevices = D3d7EnumDevices;
        table.CreateDevice = D3d7CreateDevice;
        table.CreateVertexBuffer = D3d7CreateVertexBuffer;
        table.EnumZBufferFormats = D3d7EnumZBufferFormats;
        table.EvictManagedTextures = D3d7EvictManagedTextures;
        initialized = true;
    }
    return &table;
}


// ---------------------------------------------------------------------------
// IDirect3DDevice7: slots DirectX 7 changed or added
// ---------------------------------------------------------------------------

HRESULT WINAPI Dev7QueryInterface(IDirect3DDevice7* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    HRESULT result = S_OK;
    if (IsEqualGUID(iid, IID_IDirect3DDevice7))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
    }
    else
    {
        // Anything else a device answers is what the DirectX 6 implementation
        // answers, and the pointer it returns is this same object.
        result = LegacyDirect3DDeviceVtable()->QueryInterface(LegacyDevice(self), iid, object);
    }
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirect3DDevice7::QueryInterface "
        "iid={%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} result=0x%08lx",
        iid.Data1, iid.Data2, iid.Data3,
        iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
        iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7],
        static_cast<unsigned long>(result));
    return result;
}

// The capability structure changed shape between the versions, so this reports
// the same device the enumeration published rather than forwarding.
HRESULT WINAPI Dev7GetCaps(IDirect3DDevice7*, D3DDEVICEDESC7* desc)
{
    if (desc == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    CopyFromCore(desc, re2dj::directx::CreatedDeviceDescription());
    return D3D_OK;
}

HRESULT WINAPI Dev7EnumTextureFormats(IDirect3DDevice7*,
                                      LPD3DENUMPIXELFORMATSCALLBACK callback,
                                      void* arg)
{
    if (callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DDPIXELFORMAT format = {};
    CopyFromCore(&format, re2dj::directx::Rgb565Format());
    callback(&format, arg);
    return D3D_OK;
}

// DirectX 6 reaches this work through IDirect3DViewport3, which DirectX 7
// removed in favour of device state.
HRESULT WINAPI Dev7Clear(IDirect3DDevice7* self,
                         DWORD count,
                         D3DRECT* rects,
                         DWORD flags,
                         D3DCOLOR color,
                         D3DVALUE depth,
                         DWORD stencil)
{
    return LegacyDeviceClear(
        LegacyDevice(self), count, rects, flags, color, depth, stencil);
}

HRESULT WINAPI Dev7SetViewport(IDirect3DDevice7* self, D3DVIEWPORT7* viewport)
{
    if (viewport == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    LegacyViewportState state;
    state.x = viewport->dwX;
    state.y = viewport->dwY;
    state.width = viewport->dwWidth;
    state.height = viewport->dwHeight;
    state.min_z = viewport->dvMinZ;
    state.max_z = viewport->dvMaxZ;
    return LegacyDeviceSetViewport(LegacyDevice(self), state);
}

HRESULT WINAPI Dev7GetViewport(IDirect3DDevice7* self, D3DVIEWPORT7* viewport)
{
    if (viewport == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    LegacyViewportState state;
    const HRESULT result = LegacyDeviceGetViewport(LegacyDevice(self), &state);
    if (result != D3D_OK)
    {
        return result;
    }
    viewport->dwX = state.x;
    viewport->dwY = state.y;
    viewport->dwWidth = state.width;
    viewport->dwHeight = state.height;
    viewport->dvMinZ = state.min_z;
    viewport->dvMaxZ = state.max_z;
    return D3D_OK;
}

// DirectX 7 names a texture by its surface; DirectX 6 names it by the surface's
// texture interface. Both are members of one object, so this converts and
// forwards.
HRESULT WINAPI Dev7SetTexture(IDirect3DDevice7* self, DWORD stage, IDirectDrawSurface7* surface)
{
    const IDirect3DDevice3Vtbl* const legacy = LegacyDirect3DDeviceVtable();
    if (legacy == nullptr || legacy->SetTexture == nullptr)
    {
        return DDERR_UNSUPPORTED;
    }
    IDirect3DTexture2* texture = nullptr;
    if (surface != nullptr)
    {
        texture = LegacyTextureOfSurface(LegacySurface(surface));
        if (texture == nullptr)
        {
            return DDERR_INVALIDOBJECT;
        }
    }
    IDirect3DDevice3* const device = LegacyDevice(self);
    return legacy->SetTexture(device, stage, texture);
}

HRESULT WINAPI Dev7GetTexture(IDirect3DDevice7* self, DWORD stage, IDirectDrawSurface7** surface)
{
    if (surface == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *surface = nullptr;
    const IDirect3DDevice3Vtbl* const legacy = LegacyDirect3DDeviceVtable();
    if (legacy == nullptr || legacy->GetTexture == nullptr)
    {
        return DDERR_UNSUPPORTED;
    }
    IDirect3DDevice3* const device = LegacyDevice(self);
    IDirect3DTexture2* texture = nullptr;
    const HRESULT result = legacy->GetTexture(device, stage, &texture);
    if (result != D3D_OK || texture == nullptr)
    {
        return result;
    }
    *surface = reinterpret_cast<IDirectDrawSurface7*>(LegacySurfaceOfTexture(texture));
    return *surface == nullptr ? DDERR_INVALIDOBJECT : D3D_OK;
}

// DirectX 7 takes the vertex count where DirectX 6 took none, and names the
// buffer by its own interface. The object is the same either way.
HRESULT WINAPI Dev7DrawPrimitiveVB(IDirect3DDevice7* self,
                                   D3DPRIMITIVETYPE primitive,
                                   IDirect3DVertexBuffer7* buffer,
                                   DWORD start_vertex,
                                   DWORD vertex_count,
                                   DWORD flags)
{
    const IDirect3DDevice3Vtbl* const legacy = LegacyDirect3DDeviceVtable();
    if (legacy == nullptr || legacy->DrawPrimitiveVB == nullptr)
    {
        return DDERR_UNSUPPORTED;
    }
    IDirect3DDevice3* const device = LegacyDevice(self);
    return legacy->DrawPrimitiveVB(
        device,
        primitive,
        reinterpret_cast<IDirect3DVertexBuffer*>(buffer),
        start_vertex,
        vertex_count,
        flags);
}

// DirectX 7 added a start vertex and a vertex count that DirectX 6 did not
// carry. The shared implementation indexes the whole buffer, so a request that
// starts anywhere but the first vertex is expanded here rather than silently
// drawing the wrong vertices.
HRESULT WINAPI Dev7DrawIndexedPrimitiveVB(IDirect3DDevice7* self,
                                          D3DPRIMITIVETYPE primitive,
                                          IDirect3DVertexBuffer7* buffer,
                                          DWORD start_vertex,
                                          DWORD vertex_count,
                                          WORD* indices,
                                          DWORD index_count,
                                          DWORD flags)
{
    (void)vertex_count;
    const IDirect3DDevice3Vtbl* const legacy = LegacyDirect3DDeviceVtable();
    if (legacy == nullptr || legacy->DrawIndexedPrimitiveVB == nullptr)
    {
        return DDERR_UNSUPPORTED;
    }
    if (start_vertex != 0)
    {
        static GraphicsCallLedger ledger = {"DrawIndexedPrimitiveVB:start-vertex",
                                            kUnimplementedCallBudget};
        ReportUnimplementedGraphicsCall("IDirect3DDevice7", &ledger);
        return DDERR_UNSUPPORTED;
    }
    IDirect3DDevice3* const device = LegacyDevice(self);
    return legacy->DrawIndexedPrimitiveVB(
        device,
        primitive,
        reinterpret_cast<IDirect3DVertexBuffer*>(buffer),
        indices,
        index_count,
        flags);
}

HRESULT WINAPI Dev7GetDirect3D(IDirect3DDevice7* self, IDirect3D7** direct3d)
{
    if (direct3d == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *direct3d = nullptr;
    const IDirect3DDevice3Vtbl* const legacy = LegacyDirect3DDeviceVtable();
    if (legacy == nullptr || legacy->GetDirect3D == nullptr)
    {
        return DDERR_UNSUPPORTED;
    }
    IDirect3DDevice3* const device = LegacyDevice(self);
    IDirect3D3* legacy_d3d = nullptr;
    const HRESULT result = legacy->GetDirect3D(device, &legacy_d3d);
    if (result != D3D_OK || legacy_d3d == nullptr)
    {
        return result == D3D_OK ? DDERR_INVALIDOBJECT : result;
    }
    // The DirectX 3 interface lives on the root, and the root's DirectDraw
    // interface is what the DirectX 7 Direct3D object binds to.
    IDirectDraw4* root = nullptr;
    const HRESULT root_result =
        legacy_d3d->lpVtbl->QueryInterface(legacy_d3d, IID_IDirectDraw4, reinterpret_cast<void**>(&root));
    legacy_d3d->lpVtbl->Release(legacy_d3d);
    if (root_result != S_OK || root == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    const HRESULT created = CreateDirect3D7Facade(root, reinterpret_cast<void**>(direct3d));
    root->lpVtbl->Release(root);
    return created;
}

// ---------------------------------------------------------------------------
// IDirect3DDevice7: slots with no implementation yet
// ---------------------------------------------------------------------------

#define RE2DJ_DEVICE7_UNIMPLEMENTED(name)                                     \
    do                                                                        \
    {                                                                         \
        static GraphicsCallLedger ledger = {name, kUnimplementedCallBudget};   \
        ReportUnimplementedGraphicsCall("IDirect3DDevice7", &ledger);          \
    } while (false)

HRESULT WINAPI Dev7SetMaterial(IDirect3DDevice7*, D3DMATERIAL7*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("SetMaterial");
    return D3D_OK;
}
HRESULT WINAPI Dev7GetMaterial(IDirect3DDevice7*, D3DMATERIAL7* material)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("GetMaterial");
    if (material != nullptr)
    {
        std::memset(material, 0, sizeof(*material));
    }
    return D3D_OK;
}
HRESULT WINAPI Dev7SetLight(IDirect3DDevice7*, DWORD, D3DLIGHT7*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("SetLight");
    return D3D_OK;
}
HRESULT WINAPI Dev7GetLight(IDirect3DDevice7*, DWORD, D3DLIGHT7* light)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("GetLight");
    if (light != nullptr)
    {
        std::memset(light, 0, sizeof(*light));
    }
    return D3D_OK;
}
HRESULT WINAPI Dev7LightEnable(IDirect3DDevice7*, DWORD, BOOL)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("LightEnable");
    return D3D_OK;
}
HRESULT WINAPI Dev7GetLightEnable(IDirect3DDevice7*, DWORD, BOOL* enabled)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("GetLightEnable");
    if (enabled != nullptr)
    {
        *enabled = FALSE;
    }
    return D3D_OK;
}
HRESULT WINAPI Dev7MultiplyTransform(IDirect3DDevice7*, D3DTRANSFORMSTATETYPE, D3DMATRIX*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("MultiplyTransform");
    return D3D_OK;
}
HRESULT WINAPI Dev7BeginStateBlock(IDirect3DDevice7*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("BeginStateBlock");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7EndStateBlock(IDirect3DDevice7*, DWORD* handle)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("EndStateBlock");
    if (handle != nullptr)
    {
        *handle = 0;
    }
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7ApplyStateBlock(IDirect3DDevice7*, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("ApplyStateBlock");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7CaptureStateBlock(IDirect3DDevice7*, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("CaptureStateBlock");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7DeleteStateBlock(IDirect3DDevice7*, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("DeleteStateBlock");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7CreateStateBlock(IDirect3DDevice7*, D3DSTATEBLOCKTYPE, DWORD* handle)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("CreateStateBlock");
    if (handle != nullptr)
    {
        *handle = 0;
    }
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7PreLoad(IDirect3DDevice7*, IDirectDrawSurface7*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("PreLoad");
    return D3D_OK;
}
HRESULT WINAPI Dev7Load(IDirect3DDevice7*, IDirectDrawSurface7*, POINT*, IDirectDrawSurface7*, RECT*, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("Load");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7SetClipPlane(IDirect3DDevice7*, DWORD, D3DVALUE*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("SetClipPlane");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7GetClipPlane(IDirect3DDevice7*, DWORD, D3DVALUE*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("GetClipPlane");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7GetInfo(IDirect3DDevice7*, DWORD, void*, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("GetInfo");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7DrawIndexedPrimitive(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, void*, DWORD, WORD*, DWORD, DWORD)
{
    // The DirectX 6 table leaves this slot empty as well: the 1st SE guest
    // indexes only through a vertex buffer. Whether the 4th guest reaches here
    // decides whether the shared draw path needs a second indexed entry.
    RE2DJ_DEVICE7_UNIMPLEMENTED("DrawIndexedPrimitive");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7DrawPrimitiveStrided(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, D3DDRAWPRIMITIVESTRIDEDDATA*, DWORD, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("DrawPrimitiveStrided");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7DrawIndexedPrimitiveStrided(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, D3DDRAWPRIMITIVESTRIDEDDATA*, DWORD, WORD*, DWORD, DWORD)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("DrawIndexedPrimitiveStrided");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dev7ComputeSphereVisibility(IDirect3DDevice7*, D3DVECTOR*, D3DVALUE*, DWORD count, DWORD, DWORD* results)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("ComputeSphereVisibility");
    // Reporting every sphere fully visible keeps a guest that culls by this
    // answer from dropping geometry the backend would have drawn.
    if (results != nullptr)
    {
        for (DWORD index = 0; index < count; ++index)
        {
            results[index] = 0;
        }
    }
    return D3D_OK;
}
HRESULT WINAPI Dev7SetClipStatus(IDirect3DDevice7*, D3DCLIPSTATUS*)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("SetClipStatus");
    return D3D_OK;
}
HRESULT WINAPI Dev7GetClipStatus(IDirect3DDevice7*, D3DCLIPSTATUS* status)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("GetClipStatus");
    if (status != nullptr)
    {
        std::memset(status, 0, sizeof(*status));
    }
    return D3D_OK;
}
HRESULT WINAPI Dev7ValidateDevice(IDirect3DDevice7*, DWORD* passes)
{
    RE2DJ_DEVICE7_UNIMPLEMENTED("ValidateDevice");
    if (passes != nullptr)
    {
        *passes = 1;
    }
    return D3D_OK;
}

#undef RE2DJ_DEVICE7_UNIMPLEMENTED

IDirect3DDevice7Vtbl* Device7Vtable()
{
    static IDirect3DDevice7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        const IDirect3DDevice3Vtbl* const legacy = LegacyDirect3DDeviceVtable();

        // Adopted: the DirectX 6 implementation, unchanged. These slots moved
        // within the vtable between the versions but their meaning did not.
        Adopt(table.AddRef, legacy->AddRef);
        Adopt(table.Release, legacy->Release);
        Adopt(table.BeginScene, legacy->BeginScene);
        Adopt(table.EndScene, legacy->EndScene);
        Adopt(table.SetRenderState, legacy->SetRenderState);
        Adopt(table.GetRenderState, legacy->GetRenderState);
        Adopt(table.SetTransform, legacy->SetTransform);
        Adopt(table.GetTransform, legacy->GetTransform);
        Adopt(table.SetTextureStageState, legacy->SetTextureStageState);
        Adopt(table.GetTextureStageState, legacy->GetTextureStageState);
        Adopt(table.DrawPrimitive, legacy->DrawPrimitive);
        Adopt(table.SetRenderTarget, legacy->SetRenderTarget);
        Adopt(table.GetRenderTarget, legacy->GetRenderTarget);

        // DirectX 7's own slots and the ones it changed.
        table.QueryInterface = Dev7QueryInterface;
        table.GetCaps = Dev7GetCaps;
        table.EnumTextureFormats = Dev7EnumTextureFormats;
        table.GetDirect3D = Dev7GetDirect3D;
        table.Clear = Dev7Clear;
        table.SetViewport = Dev7SetViewport;
        table.GetViewport = Dev7GetViewport;
        table.SetTexture = Dev7SetTexture;
        table.GetTexture = Dev7GetTexture;
        table.DrawPrimitiveVB = Dev7DrawPrimitiveVB;
        table.DrawIndexedPrimitiveVB = Dev7DrawIndexedPrimitiveVB;

        // No implementation yet; each records the calls it receives.
        table.SetMaterial = Dev7SetMaterial;
        table.GetMaterial = Dev7GetMaterial;
        table.SetLight = Dev7SetLight;
        table.GetLight = Dev7GetLight;
        table.LightEnable = Dev7LightEnable;
        table.GetLightEnable = Dev7GetLightEnable;
        table.MultiplyTransform = Dev7MultiplyTransform;
        table.BeginStateBlock = Dev7BeginStateBlock;
        table.EndStateBlock = Dev7EndStateBlock;
        table.ApplyStateBlock = Dev7ApplyStateBlock;
        table.CaptureStateBlock = Dev7CaptureStateBlock;
        table.DeleteStateBlock = Dev7DeleteStateBlock;
        table.CreateStateBlock = Dev7CreateStateBlock;
        table.PreLoad = Dev7PreLoad;
        table.Load = Dev7Load;
        table.SetClipPlane = Dev7SetClipPlane;
        table.GetClipPlane = Dev7GetClipPlane;
        table.GetInfo = Dev7GetInfo;
        table.DrawIndexedPrimitive = Dev7DrawIndexedPrimitive;
        table.DrawPrimitiveStrided = Dev7DrawPrimitiveStrided;
        table.DrawIndexedPrimitiveStrided = Dev7DrawIndexedPrimitiveStrided;
        table.ComputeSphereVisibility = Dev7ComputeSphereVisibility;
        table.SetClipStatus = Dev7SetClipStatus;
        table.GetClipStatus = Dev7GetClipStatus;
        table.ValidateDevice = Dev7ValidateDevice;
        initialized = true;
    }
    return &table;
}

}  // namespace

HRESULT CreateDirect3D7Facade(IDirectDraw4* root, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    if (root == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    auto* const facade = new (std::nothrow) Direct3D7Facade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->interface_value.lpVtbl = D3D7Vtable();
    facade->root = root;
    // The Direct3D interface keeps the root alive for as long as the guest
    // holds it, the way QueryInterface on a DirectDraw object does.
    root->lpVtbl->AddRef(root);
    *object = &facade->interface_value;
    return S_OK;
}

const IDirect3DDevice3Vtbl* Direct3DDevice7VtableAsLegacy()
{
    return reinterpret_cast<const IDirect3DDevice3Vtbl*>(Device7Vtable());
}

}  // namespace re2dj::platform::windows
