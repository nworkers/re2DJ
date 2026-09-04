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
#include "graphics_trace_log.h"

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

// Fills the primitive caps a DirectX 7 title inspects before it accepts a
// device. A guest that walks these fields rejects a device whose caps are left
// zero, so every capability the legacy OpenGL backend can honour is reported.
void FillPrimitiveCaps(D3DPRIMCAPS* caps)
{
    std::memset(caps, 0, sizeof(*caps));
    caps->dwSize = sizeof(*caps);
    caps->dwMiscCaps = D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW |
                       D3DPMISCCAPS_CULLCCW | D3DPMISCCAPS_MASKZ;
    caps->dwRasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_SUBPIXEL |
                         D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX |
                         D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_ZBIAS |
                         D3DPRASTERCAPS_WBUFFER | D3DPRASTERCAPS_WFOG |
                         D3DPRASTERCAPS_ZFOG;
    caps->dwZCmpCaps = D3DPCMPCAPS_NEVER | D3DPCMPCAPS_LESS | D3DPCMPCAPS_EQUAL |
                       D3DPCMPCAPS_LESSEQUAL | D3DPCMPCAPS_GREATER |
                       D3DPCMPCAPS_NOTEQUAL | D3DPCMPCAPS_GREATEREQUAL |
                       D3DPCMPCAPS_ALWAYS;
    caps->dwSrcBlendCaps = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE |
                           D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_INVSRCCOLOR |
                           D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_INVSRCALPHA |
                           D3DPBLENDCAPS_DESTALPHA | D3DPBLENDCAPS_INVDESTALPHA |
                           D3DPBLENDCAPS_DESTCOLOR | D3DPBLENDCAPS_INVDESTCOLOR |
                           D3DPBLENDCAPS_SRCALPHASAT;
    caps->dwDestBlendCaps = caps->dwSrcBlendCaps;
    caps->dwAlphaCmpCaps = caps->dwZCmpCaps;
    caps->dwShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB |
                        D3DPSHADECAPS_SPECULARGOURAUDRGB |
                        D3DPSHADECAPS_ALPHAGOURAUDBLEND |
                        D3DPSHADECAPS_FOGGOURAUD;
    caps->dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA |
                          D3DPTEXTURECAPS_TRANSPARENCY |
                          D3DPTEXTURECAPS_ALPHAPALETTE;
    caps->dwTextureFilterCaps =
        D3DPTFILTERCAPS_NEAREST | D3DPTFILTERCAPS_LINEAR |
        D3DPTFILTERCAPS_MIPNEAREST | D3DPTFILTERCAPS_MIPLINEAR |
        D3DPTFILTERCAPS_LINEARMIPNEAREST | D3DPTFILTERCAPS_LINEARMIPLINEAR |
        D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR |
        D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR |
        D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR;
    caps->dwTextureBlendCaps = D3DPTBLENDCAPS_DECAL | D3DPTBLENDCAPS_MODULATE |
                               D3DPTBLENDCAPS_DECALALPHA |
                               D3DPTBLENDCAPS_MODULATEALPHA |
                               D3DPTBLENDCAPS_COPY | D3DPTBLENDCAPS_ADD;
    caps->dwTextureAddressCaps =
        D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR |
        D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_BORDER |
        D3DPTADDRESSCAPS_INDEPENDENTUV;
}

void FillDeviceDescription(D3DDEVICEDESC7* desc, const GUID& device_guid, bool hardware_tnl)
{
    std::memset(desc, 0, sizeof(*desc));
    desc->dwDevCaps = D3DDEVCAPS_FLOATTLVERTEX | D3DDEVCAPS_EXECUTESYSTEMMEMORY |
                      D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
                      D3DDEVCAPS_TEXTURESYSTEMMEMORY |
                      D3DDEVCAPS_TEXTUREVIDEOMEMORY |
                      D3DDEVCAPS_DRAWPRIMTLVERTEX |
                      D3DDEVCAPS_CANRENDERAFTERFLIP |
                      D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX |
                      D3DDEVCAPS_HWRASTERIZATION;
    if (hardware_tnl)
    {
        desc->dwDevCaps |= D3DDEVCAPS_HWTRANSFORMANDLIGHT;
    }
    FillPrimitiveCaps(&desc->dpcLineCaps);
    FillPrimitiveCaps(&desc->dpcTriCaps);
    desc->dwDeviceRenderBitDepth = DDBD_16 | DDBD_24 | DDBD_32;
    desc->dwDeviceZBufferBitDepth = DDBD_16 | DDBD_24 | DDBD_32;
    desc->dwMinTextureWidth = 1;
    desc->dwMinTextureHeight = 1;
    desc->dwMaxTextureWidth = 2048;
    desc->dwMaxTextureHeight = 2048;
    desc->dwMaxTextureRepeat = 2048;
    desc->dwMaxTextureAspectRatio = 2048;
    desc->dwMaxAnisotropy = 1;
    desc->dvGuardBandLeft = -32768.0f;
    desc->dvGuardBandTop = -32768.0f;
    desc->dvGuardBandRight = 32768.0f;
    desc->dvGuardBandBottom = 32768.0f;
    desc->dvExtentsAdjust = 0.0f;
    desc->dwStencilCaps = 0;
    // The low bits of dwFVFCaps report how many texture coordinate sets the
    // device accepts in a flexible vertex format.
    desc->dwFVFCaps = 8;
    desc->dwTextureOpCaps =
        D3DTEXOPCAPS_DISABLE | D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_SELECTARG2 |
        D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_MODULATE2X | D3DTEXOPCAPS_MODULATE4X |
        D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_ADDSIGNED2X |
        D3DTEXOPCAPS_SUBTRACT | D3DTEXOPCAPS_ADDSMOOTH |
        D3DTEXOPCAPS_BLENDDIFFUSEALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHA |
        D3DTEXOPCAPS_BLENDFACTORALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM |
        D3DTEXOPCAPS_BLENDCURRENTALPHA;
    desc->wMaxTextureBlendStages = 8;
    desc->wMaxSimultaneousTextures = 8;
    desc->dwMaxActiveLights = 8;
    desc->dvMaxVertexW = 1.0e10f;
    desc->deviceGUID = device_guid;
    desc->wMaxUserClipPlanes = 6;
    desc->wMaxVertexBlendMatrices = 1;
    desc->dwVertexProcessingCaps =
        D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 | D3DVTXPCAPS_VERTEXFOG |
        D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS |
        D3DVTXPCAPS_LOCALVIEWER;
}

HRESULT WINAPI D3d7EnumDevices(IDirect3D7* self, LPD3DENUMDEVICESCALLBACK7 callback, void* arg)
{
    (void)self;
    WriteGraphicsTraceFormat("re2dj:hle:IDirect3D7::EnumDevices callback=%p", callback);
    if (callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    // DirectX 7 enumerates the software rasterizer first and the most capable
    // hardware device last, and titles commonly pick by walking that order.
    // The names match the retail DirectX 7 strings because guests are known to
    // select a device by comparing them.
    struct EnumeratedDevice
    {
        const char* description;
        const char* name;
        const GUID* device_guid;
        bool hardware_tnl;
    };
    const EnumeratedDevice devices[] = {
        {"Microsoft Direct3D RGB Software Emulation",
         "RGB Emulation",
         &IID_IDirect3DRGBDevice,
         false},
        {"Microsoft Direct3D Hardware acceleration through Direct3D HAL",
         "Direct3D HAL",
         &IID_IDirect3DHALDevice,
         false},
        {"Microsoft Direct3D Hardware Transform and Lighting acceleration capable device",
         "Direct3D T&L HAL",
         &IID_IDirect3DTnLHalDevice,
         true},
    };

    for (const EnumeratedDevice& device : devices)
    {
        D3DDEVICEDESC7 desc = {};
        FillDeviceDescription(&desc, *device.device_guid, device.hardware_tnl);
        char description[128] = {};
        char name[64] = {};
        std::snprintf(description, sizeof(description), "%s", device.description);
        std::snprintf(name, sizeof(name), "%s", device.name);
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
    // Provide 16-bit D16 format
    DDPIXELFORMAT ddpf = {};
    ddpf.dwSize = sizeof(ddpf);
    ddpf.dwFlags = DDPF_ZBUFFER;
    ddpf.dwZBufferBitDepth = 16;
    ddpf.dwZBitMask = 0x0000ffff;
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
    FillDeviceDescription(desc, IID_IDirect3DHALDevice, false);
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
    // The shared surface backing stores one layout, so one format is offered.
    DDPIXELFORMAT format = {};
    format.dwSize = sizeof(format);
    format.dwFlags = DDPF_RGB;
    format.dwRGBBitCount = 16;
    format.dwRBitMask = 0xf800;
    format.dwGBitMask = 0x07e0;
    format.dwBBitMask = 0x001f;
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
