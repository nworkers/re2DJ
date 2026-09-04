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
#include "direct3d7_vertex_buffer_facade.h"
#include "directdraw_com_context.h"
#include "graphics_trace_log.h"

namespace re2dj::platform::windows
{
namespace
{

struct Direct3D7Facade;
struct Device7Facade;

struct Direct3D7Facade
{
    IDirect3D7 interface_value;
    volatile LONG references = 1;
    DirectDrawComContext* context = nullptr;
};

struct Device7Facade
{
    IDirect3DDevice7 interface_value;
    volatile LONG references = 1;
    DirectDrawComContext* context = nullptr;
};

// Forward declarations
IDirect3D7Vtbl* D3D7Vtable();
IDirect3DDevice7Vtbl* Device7Vtable();

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
        delete facade;
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

HRESULT WINAPI D3d7CreateDevice(IDirect3D7* self,
                               REFCLSID rclsid,
                               IDirectDrawSurface7* surface,
                               IDirect3DDevice7** device)
{
    (void)rclsid;
    (void)surface;
    WriteGraphicsTraceLine("re2dj:hle:IDirect3D7::CreateDevice");
    if (device == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *device = nullptr;
    auto* const facade = reinterpret_cast<Direct3D7Facade*>(self);
    auto* const dev_facade = new (std::nothrow) Device7Facade;
    if (dev_facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    dev_facade->interface_value.lpVtbl = Device7Vtable();
    dev_facade->context = facade->context;
    *device = &dev_facade->interface_value;
    return D3D_OK;
}

HRESULT WINAPI D3d7CreateVertexBuffer(IDirect3D7* self,
                                     D3DVERTEXBUFFERDESC* desc,
                                     IDirect3DVertexBuffer7** vb,
                                     DWORD flags)
{
    (void)flags;
    auto* const facade = reinterpret_cast<Direct3D7Facade*>(self);
    return CreateDirect3DVertexBuffer7Facade(facade == nullptr ? nullptr : facade->context,
                                             desc,
                                             vb);
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

// Device7 methods
HRESULT WINAPI Dev7QueryInterface(IDirect3DDevice7* self, REFIID iid, void** object)
{
    if (object == nullptr) return E_POINTER;
    *object = nullptr;
    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IDirect3DDevice7))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG WINAPI Dev7AddRef(IDirect3DDevice7* self)
{
    auto* const facade = reinterpret_cast<Device7Facade*>(self);
    return static_cast<ULONG>(InterlockedIncrement(&facade->references));
}

ULONG WINAPI Dev7Release(IDirect3DDevice7* self)
{
    auto* const facade = reinterpret_cast<Device7Facade*>(self);
    const LONG count = InterlockedDecrement(&facade->references);
    if (count <= 0)
    {
        delete facade;
        return 0;
    }
    return static_cast<ULONG>(count);
}

HRESULT WINAPI Dev7GetCaps(IDirect3DDevice7*, D3DDEVICEDESC7* desc)
{
    if (desc != nullptr)
    {
        std::memset(desc, 0, sizeof(*desc));
        desc->dwDevCaps = D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_HWTRANSFORMANDLIGHT;
    }
    return D3D_OK;
}

HRESULT WINAPI Dev7EnumTextureFormats(IDirect3DDevice7*, LPD3DENUMPIXELFORMATSCALLBACK callback, void* arg)
{
    if (callback == nullptr) return DDERR_INVALIDPARAMS;
    DDPIXELFORMAT pf = {};
    pf.dwSize = sizeof(pf);
    pf.dwFlags = DDPF_RGB;
    pf.dwRGBBitCount = 16;
    pf.dwRBitMask = 0xf800;
    pf.dwGBitMask = 0x07e0;
    pf.dwBBitMask = 0x001f;
    callback(&pf, arg);
    return D3D_OK;
}

HRESULT WINAPI Dev7BeginScene(IDirect3DDevice7*) { return D3D_OK; }
HRESULT WINAPI Dev7EndScene(IDirect3DDevice7*) { return D3D_OK; }
HRESULT WINAPI Dev7GetDirect3D(IDirect3DDevice7* self, IDirect3D7** d3d)
{
    if (d3d == nullptr) return DDERR_INVALIDPARAMS;
    auto* const facade = reinterpret_cast<Device7Facade*>(self);
    return CreateDirect3D7Facade(facade->context, reinterpret_cast<void**>(d3d));
}
HRESULT WINAPI Dev7SetRenderTarget(IDirect3DDevice7*, IDirectDrawSurface7*, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7GetRenderTarget(IDirect3DDevice7*, IDirectDrawSurface7** rt)
{
    if (rt != nullptr) *rt = nullptr;
    return D3D_OK;
}
HRESULT WINAPI Dev7Clear(IDirect3DDevice7*, DWORD, D3DRECT*, DWORD, D3DCOLOR, D3DVALUE, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7SetTransform(IDirect3DDevice7*, D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return D3D_OK; }
HRESULT WINAPI Dev7GetTransform(IDirect3DDevice7*, D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return D3D_OK; }
HRESULT WINAPI Dev7SetViewport(IDirect3DDevice7*, D3DVIEWPORT7*) { return D3D_OK; }
HRESULT WINAPI Dev7MultiplyTransform(IDirect3DDevice7*, D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return D3D_OK; }
HRESULT WINAPI Dev7GetViewport(IDirect3DDevice7*, D3DVIEWPORT7*) { return D3D_OK; }
HRESULT WINAPI Dev7SetMaterial(IDirect3DDevice7*, D3DMATERIAL7*) { return D3D_OK; }
HRESULT WINAPI Dev7GetMaterial(IDirect3DDevice7*, D3DMATERIAL7*) { return D3D_OK; }
HRESULT WINAPI Dev7SetLight(IDirect3DDevice7*, DWORD, D3DLIGHT7*) { return D3D_OK; }
HRESULT WINAPI Dev7GetLight(IDirect3DDevice7*, DWORD, D3DLIGHT7*) { return D3D_OK; }
HRESULT WINAPI Dev7SetRenderState(IDirect3DDevice7*, D3DRENDERSTATETYPE, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7GetRenderState(IDirect3DDevice7*, D3DRENDERSTATETYPE, DWORD* val)
{
    if (val != nullptr) *val = 0;
    return D3D_OK;
}
HRESULT WINAPI Dev7BeginStateBlock(IDirect3DDevice7*) { return D3D_OK; }
HRESULT WINAPI Dev7EndStateBlock(IDirect3DDevice7*, DWORD*) { return D3D_OK; }
HRESULT WINAPI Dev7PreLoad(IDirect3DDevice7*, IDirectDrawSurface7*) { return D3D_OK; }
HRESULT WINAPI Dev7DrawPrimitive(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, void*, DWORD, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7DrawIndexedPrimitive(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, void*, DWORD, WORD*, DWORD, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7SetClipStatus(IDirect3DDevice7*, D3DCLIPSTATUS*) { return D3D_OK; }
HRESULT WINAPI Dev7GetClipStatus(IDirect3DDevice7*, D3DCLIPSTATUS*) { return D3D_OK; }
HRESULT WINAPI Dev7DrawPrimitiveStrided(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, D3DDRAWPRIMITIVESTRIDEDDATA*, DWORD, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7DrawIndexedPrimitiveStrided(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD, D3DDRAWPRIMITIVESTRIDEDDATA*, DWORD, WORD*, DWORD, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7DrawPrimitiveVB(IDirect3DDevice7*, D3DPRIMITIVETYPE, IDirect3DVertexBuffer7*, DWORD, DWORD, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7DrawIndexedPrimitiveVB(IDirect3DDevice7*, D3DPRIMITIVETYPE, IDirect3DVertexBuffer7*, DWORD, DWORD, WORD*, DWORD, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7ComputeSphereVisibility(IDirect3DDevice7*, D3DVECTOR*, D3DVALUE*, DWORD, DWORD, DWORD*) { return D3D_OK; }
HRESULT WINAPI Dev7GetTexture(IDirect3DDevice7*, DWORD, IDirectDrawSurface7** tex)
{
    if (tex != nullptr) *tex = nullptr;
    return D3D_OK;
}
HRESULT WINAPI Dev7SetTexture(IDirect3DDevice7*, DWORD, IDirectDrawSurface7*) { return D3D_OK; }
HRESULT WINAPI Dev7GetTextureStageState(IDirect3DDevice7*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD* val)
{
    if (val != nullptr) *val = 0;
    return D3D_OK;
}
HRESULT WINAPI Dev7SetTextureStageState(IDirect3DDevice7*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7ValidateDevice(IDirect3DDevice7*, DWORD*) { return D3D_OK; }
HRESULT WINAPI Dev7ApplyStateBlock(IDirect3DDevice7*, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7CaptureStateBlock(IDirect3DDevice7*, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7DeleteStateBlock(IDirect3DDevice7*, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7CreateStateBlock(IDirect3DDevice7*, D3DSTATEBLOCKTYPE, DWORD*) { return D3D_OK; }
HRESULT WINAPI Dev7Load(IDirect3DDevice7*, IDirectDrawSurface7*, POINT*, IDirectDrawSurface7*, RECT*, DWORD) { return D3D_OK; }
HRESULT WINAPI Dev7LightEnable(IDirect3DDevice7*, DWORD, BOOL) { return D3D_OK; }
HRESULT WINAPI Dev7GetLightEnable(IDirect3DDevice7*, DWORD, BOOL* en)
{
    if (en != nullptr) *en = FALSE;
    return D3D_OK;
}
HRESULT WINAPI Dev7SetClipPlane(IDirect3DDevice7*, DWORD, D3DVALUE*) { return D3D_OK; }
HRESULT WINAPI Dev7GetClipPlane(IDirect3DDevice7*, DWORD, D3DVALUE*) { return D3D_OK; }
HRESULT WINAPI Dev7GetInfo(IDirect3DDevice7*, DWORD, void*, DWORD) { return D3D_OK; }

IDirect3DDevice7Vtbl* Device7Vtable()
{
    static IDirect3DDevice7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = Dev7QueryInterface;
        table.AddRef = Dev7AddRef;
        table.Release = Dev7Release;
        table.GetCaps = Dev7GetCaps;
        table.EnumTextureFormats = Dev7EnumTextureFormats;
        table.BeginScene = Dev7BeginScene;
        table.EndScene = Dev7EndScene;
        table.GetDirect3D = Dev7GetDirect3D;
        table.SetRenderTarget = Dev7SetRenderTarget;
        table.GetRenderTarget = Dev7GetRenderTarget;
        table.Clear = Dev7Clear;
        table.SetTransform = Dev7SetTransform;
        table.GetTransform = Dev7GetTransform;
        table.SetViewport = Dev7SetViewport;
        table.MultiplyTransform = Dev7MultiplyTransform;
        table.GetViewport = Dev7GetViewport;
        table.SetMaterial = Dev7SetMaterial;
        table.GetMaterial = Dev7GetMaterial;
        table.SetLight = Dev7SetLight;
        table.GetLight = Dev7GetLight;
        table.SetRenderState = Dev7SetRenderState;
        table.GetRenderState = Dev7GetRenderState;
        table.BeginStateBlock = Dev7BeginStateBlock;
        table.EndStateBlock = Dev7EndStateBlock;
        table.PreLoad = Dev7PreLoad;
        table.DrawPrimitive = Dev7DrawPrimitive;
        table.DrawIndexedPrimitive = Dev7DrawIndexedPrimitive;
        table.SetClipStatus = Dev7SetClipStatus;
        table.GetClipStatus = Dev7GetClipStatus;
        table.DrawPrimitiveStrided = Dev7DrawPrimitiveStrided;
        table.DrawIndexedPrimitiveStrided = Dev7DrawIndexedPrimitiveStrided;
        table.DrawPrimitiveVB = Dev7DrawPrimitiveVB;
        table.DrawIndexedPrimitiveVB = Dev7DrawIndexedPrimitiveVB;
        table.ComputeSphereVisibility = Dev7ComputeSphereVisibility;
        table.GetTexture = Dev7GetTexture;
        table.SetTexture = Dev7SetTexture;
        table.GetTextureStageState = Dev7GetTextureStageState;
        table.SetTextureStageState = Dev7SetTextureStageState;
        table.ValidateDevice = Dev7ValidateDevice;
        table.ApplyStateBlock = Dev7ApplyStateBlock;
        table.CaptureStateBlock = Dev7CaptureStateBlock;
        table.DeleteStateBlock = Dev7DeleteStateBlock;
        table.CreateStateBlock = Dev7CreateStateBlock;
        table.Load = Dev7Load;
        table.LightEnable = Dev7LightEnable;
        table.GetLightEnable = Dev7GetLightEnable;
        table.SetClipPlane = Dev7SetClipPlane;
        table.GetClipPlane = Dev7GetClipPlane;
        table.GetInfo = Dev7GetInfo;
        initialized = true;
    }
    return &table;
}

}  // namespace

HRESULT CreateDirect3D7Facade(DirectDrawComContext* context, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    auto* const facade = new (std::nothrow) Direct3D7Facade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->interface_value.lpVtbl = D3D7Vtable();
    facade->context = context;
    *object = &facade->interface_value;
    return S_OK;
}

}  // namespace re2dj::platform::windows
