#define NOMINMAX
#define CINTERFACE
#define INITGUID
#define DIRECT3D_VERSION 0x0600
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <string>

#include "direct3d3_opengl_backend.h"
#include "re2dj/graphics/legacy_draw_command.h"

namespace
{

constexpr char kDirectDrawCreateMessage[] = "re2dj:hle:DirectDrawCreate";
constexpr char kFindDeviceMessage[] = "re2dj:hle:IDirect3D3::FindDevice";
constexpr char kCreateDeviceMessage[] = "re2dj:hle:IDirect3D3::CreateDevice";
constexpr char kCreateTextureSurfaceMessage[] =
    "re2dj:hle:IDirectDraw4::CreateTextureSurface";
constexpr char kDrawPrimitiveMessage[] = "re2dj:hle:IDirect3DDevice3::DrawPrimitive";
constexpr char kOpenGlFailureMessage[] = "re2dj:hle:OpenGLFailure";
constexpr DWORD kRootMagic = 0x52324444;
constexpr DWORD kSurfaceMagic = 0x52325346;
constexpr DWORD kDeviceMagic = 0x52324456;
constexpr DWORD kViewportMagic = 0x52325650;

struct RootFacade;
struct SurfaceFacade;
struct DeviceFacade;
struct ViewportFacade;

HRESULT WINAPI RootQueryInterface(IDirectDraw4* self, REFIID iid, void** object);
ULONG WINAPI RootAddRef(IDirectDraw4* self);
ULONG WINAPI RootRelease(IDirectDraw4* self);
HRESULT WINAPI RootGetCaps(IDirectDraw4* self, DDCAPS* driver_caps, DDCAPS* hel_caps);
HRESULT WINAPI RootCreateSurface(IDirectDraw4* self,
                                 DDSURFACEDESC2* descriptor,
                                 IDirectDrawSurface4** surface,
                                 IUnknown* outer);
HRESULT WINAPI RootSetCooperativeLevel(IDirectDraw4* self, HWND window, DWORD flags);
HRESULT WINAPI RootSetDisplayMode(IDirectDraw4* self,
                                  DWORD width,
                                  DWORD height,
                                  DWORD bits_per_pixel,
                                  DWORD refresh_rate,
                                  DWORD flags);
HRESULT WINAPI RootRestoreDisplayMode(IDirectDraw4* self);
HRESULT WINAPI RootRestoreAllSurfaces(IDirectDraw4* self);

HRESULT WINAPI D3dQueryInterface(IDirect3D3* self, REFIID iid, void** object);
ULONG WINAPI D3dAddRef(IDirect3D3* self);
ULONG WINAPI D3dRelease(IDirect3D3* self);
HRESULT WINAPI D3dCreateViewport(IDirect3D3* self,
                                 IDirect3DViewport3** viewport,
                                 IUnknown* outer);
HRESULT WINAPI D3dFindDevice(IDirect3D3* self,
                             D3DFINDDEVICESEARCH* search,
                             D3DFINDDEVICERESULT* result);
HRESULT WINAPI D3dCreateDevice(IDirect3D3* self,
                               REFCLSID device_class,
                               IDirectDrawSurface4* render_target,
                               IDirect3DDevice3** device,
                               IUnknown* outer);
HRESULT WINAPI D3dEnumZBufferFormats(IDirect3D3* self,
                                     REFCLSID device_class,
                                     LPD3DENUMPIXELFORMATSCALLBACK callback,
                                     void* context);

HRESULT WINAPI SurfaceQueryInterface(IDirectDrawSurface4* self, REFIID iid, void** object);
ULONG WINAPI SurfaceAddRef(IDirectDrawSurface4* self);
ULONG WINAPI SurfaceRelease(IDirectDrawSurface4* self);
HRESULT WINAPI SurfaceBlt(IDirectDrawSurface4* self,
                          RECT* destination,
                          IDirectDrawSurface4* source,
                          RECT* source_rectangle,
                          DWORD flags,
                          DDBLTFX* effects);
HRESULT WINAPI SurfaceFlip(IDirectDrawSurface4* self,
                           IDirectDrawSurface4* override_surface,
                           DWORD flags);
HRESULT WINAPI SurfaceGetAttachedSurface(IDirectDrawSurface4* self,
                                         DDSCAPS2* capabilities,
                                         IDirectDrawSurface4** surface);
HRESULT WINAPI SurfaceGetCaps(IDirectDrawSurface4* self, DDSCAPS2* capabilities);
HRESULT WINAPI SurfaceGetPixelFormat(IDirectDrawSurface4* self, DDPIXELFORMAT* format);
HRESULT WINAPI SurfaceGetSurfaceDesc(IDirectDrawSurface4* self, DDSURFACEDESC2* descriptor);
HRESULT WINAPI SurfaceGetDC(IDirectDrawSurface4* self, HDC* dc);
HRESULT WINAPI SurfaceIsLost(IDirectDrawSurface4* self);
HRESULT WINAPI SurfaceReleaseDC(IDirectDrawSurface4* self, HDC dc);
HRESULT WINAPI SurfaceRestore(IDirectDrawSurface4* self);
HRESULT WINAPI SurfaceSetColorKey(IDirectDrawSurface4* self,
                                  DWORD flags,
                                  DDCOLORKEY* color_key);

HRESULT WINAPI TextureQueryInterface(IDirect3DTexture2* self, REFIID iid, void** object);
ULONG WINAPI TextureAddRef(IDirect3DTexture2* self);
ULONG WINAPI TextureRelease(IDirect3DTexture2* self);
HRESULT WINAPI TextureGetHandle(IDirect3DTexture2* self,
                                IDirect3DDevice2* device,
                                D3DTEXTUREHANDLE* handle);
HRESULT WINAPI TexturePaletteChanged(IDirect3DTexture2* self,
                                     DWORD start,
                                     DWORD count);
HRESULT WINAPI TextureLoad(IDirect3DTexture2* self, IDirect3DTexture2* source);

HRESULT WINAPI DeviceQueryInterface(IDirect3DDevice3* self, REFIID iid, void** object);
ULONG WINAPI DeviceAddRef(IDirect3DDevice3* self);
ULONG WINAPI DeviceRelease(IDirect3DDevice3* self);
HRESULT WINAPI DeviceGetCaps(IDirect3DDevice3* self,
                             D3DDEVICEDESC* hardware,
                             D3DDEVICEDESC* software);
HRESULT WINAPI DeviceAddViewport(IDirect3DDevice3* self, IDirect3DViewport3* viewport);
HRESULT WINAPI DeviceDeleteViewport(IDirect3DDevice3* self, IDirect3DViewport3* viewport);
HRESULT WINAPI DeviceBeginScene(IDirect3DDevice3* self);
HRESULT WINAPI DeviceEndScene(IDirect3DDevice3* self);
HRESULT WINAPI DeviceEnumTextureFormats(IDirect3DDevice3* self,
                                        LPD3DENUMPIXELFORMATSCALLBACK callback,
                                        void* context);
HRESULT WINAPI DeviceGetDirect3D(IDirect3DDevice3* self, IDirect3D3** direct3d);
HRESULT WINAPI DeviceSetCurrentViewport(IDirect3DDevice3* self, IDirect3DViewport3* viewport);
HRESULT WINAPI DeviceGetCurrentViewport(IDirect3DDevice3* self, IDirect3DViewport3** viewport);
HRESULT WINAPI DeviceGetRenderState(IDirect3DDevice3* self,
                                    D3DRENDERSTATETYPE state,
                                    DWORD* value);
HRESULT WINAPI DeviceSetRenderState(IDirect3DDevice3* self,
                                    D3DRENDERSTATETYPE state,
                                    DWORD value);
HRESULT WINAPI DeviceGetLightState(IDirect3DDevice3* self,
                                   D3DLIGHTSTATETYPE state,
                                   DWORD* value);
HRESULT WINAPI DeviceSetLightState(IDirect3DDevice3* self,
                                   D3DLIGHTSTATETYPE state,
                                   DWORD value);
HRESULT WINAPI DeviceSetTransform(IDirect3DDevice3* self,
                                  D3DTRANSFORMSTATETYPE state,
                                  D3DMATRIX* matrix);
HRESULT WINAPI DeviceGetTransform(IDirect3DDevice3* self,
                                  D3DTRANSFORMSTATETYPE state,
                                  D3DMATRIX* matrix);
HRESULT WINAPI DeviceGetTexture(IDirect3DDevice3* self,
                                DWORD stage,
                                IDirect3DTexture2** texture);
HRESULT WINAPI DeviceSetTexture(IDirect3DDevice3* self,
                                DWORD stage,
                                IDirect3DTexture2* texture);
HRESULT WINAPI DeviceGetTextureStageState(IDirect3DDevice3* self,
                                          DWORD stage,
                                          D3DTEXTURESTAGESTATETYPE state,
                                          DWORD* value);
HRESULT WINAPI DeviceSetTextureStageState(IDirect3DDevice3* self,
                                          DWORD stage,
                                          D3DTEXTURESTAGESTATETYPE state,
                                          DWORD value);
HRESULT WINAPI DeviceDrawPrimitive(IDirect3DDevice3* self,
                                   D3DPRIMITIVETYPE primitive,
                                   DWORD vertex_type,
                                   void* vertices,
                                   DWORD vertex_count,
                                   DWORD flags);

HRESULT WINAPI ViewportQueryInterface(IDirect3DViewport3* self, REFIID iid, void** object);
ULONG WINAPI ViewportAddRef(IDirect3DViewport3* self);
ULONG WINAPI ViewportRelease(IDirect3DViewport3* self);
HRESULT WINAPI ViewportGetViewport2(IDirect3DViewport3* self, D3DVIEWPORT2* viewport);
HRESULT WINAPI ViewportSetViewport2(IDirect3DViewport3* self, D3DVIEWPORT2* viewport);

IDirectDraw4Vtbl* DirectDrawVtable()
{
    static IDirectDraw4Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = RootQueryInterface;
        table.AddRef = RootAddRef;
        table.Release = RootRelease;
        table.GetCaps = RootGetCaps;
        table.CreateSurface = RootCreateSurface;
        table.SetCooperativeLevel = RootSetCooperativeLevel;
        table.SetDisplayMode = RootSetDisplayMode;
        table.RestoreDisplayMode = RootRestoreDisplayMode;
        table.RestoreAllSurfaces = RootRestoreAllSurfaces;
        initialized = true;
    }
    return &table;
}

IDirect3D3Vtbl* Direct3dVtable()
{
    static IDirect3D3Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = D3dQueryInterface;
        table.AddRef = D3dAddRef;
        table.Release = D3dRelease;
        table.CreateViewport = D3dCreateViewport;
        table.FindDevice = D3dFindDevice;
        table.CreateDevice = D3dCreateDevice;
        table.EnumZBufferFormats = D3dEnumZBufferFormats;
        initialized = true;
    }
    return &table;
}

IDirectDrawSurface4Vtbl* SurfaceVtable()
{
    static IDirectDrawSurface4Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = SurfaceQueryInterface;
        table.AddRef = SurfaceAddRef;
        table.Release = SurfaceRelease;
        table.Blt = SurfaceBlt;
        table.Flip = SurfaceFlip;
        table.GetAttachedSurface = SurfaceGetAttachedSurface;
        table.GetCaps = SurfaceGetCaps;
        table.GetDC = SurfaceGetDC;
        table.GetPixelFormat = SurfaceGetPixelFormat;
        table.GetSurfaceDesc = SurfaceGetSurfaceDesc;
        table.IsLost = SurfaceIsLost;
        table.ReleaseDC = SurfaceReleaseDC;
        table.Restore = SurfaceRestore;
        table.SetColorKey = SurfaceSetColorKey;
        initialized = true;
    }
    return &table;
}

IDirect3DTexture2Vtbl* TextureVtable()
{
    static IDirect3DTexture2Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = TextureQueryInterface;
        table.AddRef = TextureAddRef;
        table.Release = TextureRelease;
        table.GetHandle = TextureGetHandle;
        table.PaletteChanged = TexturePaletteChanged;
        table.Load = TextureLoad;
        initialized = true;
    }
    return &table;
}

IDirect3DDevice3Vtbl* DeviceVtable()
{
    static IDirect3DDevice3Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = DeviceQueryInterface;
        table.AddRef = DeviceAddRef;
        table.Release = DeviceRelease;
        table.GetCaps = DeviceGetCaps;
        table.AddViewport = DeviceAddViewport;
        table.DeleteViewport = DeviceDeleteViewport;
        table.EnumTextureFormats = DeviceEnumTextureFormats;
        table.BeginScene = DeviceBeginScene;
        table.EndScene = DeviceEndScene;
        table.GetDirect3D = DeviceGetDirect3D;
        table.SetCurrentViewport = DeviceSetCurrentViewport;
        table.GetCurrentViewport = DeviceGetCurrentViewport;
        table.GetRenderState = DeviceGetRenderState;
        table.SetRenderState = DeviceSetRenderState;
        table.GetLightState = DeviceGetLightState;
        table.SetLightState = DeviceSetLightState;
        table.SetTransform = DeviceSetTransform;
        table.GetTransform = DeviceGetTransform;
        table.GetTexture = DeviceGetTexture;
        table.SetTexture = DeviceSetTexture;
        table.GetTextureStageState = DeviceGetTextureStageState;
        table.SetTextureStageState = DeviceSetTextureStageState;
        table.DrawPrimitive = DeviceDrawPrimitive;
        initialized = true;
    }
    return &table;
}

IDirect3DViewport3Vtbl* ViewportVtable()
{
    static IDirect3DViewport3Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = ViewportQueryInterface;
        table.AddRef = ViewportAddRef;
        table.Release = ViewportRelease;
        table.GetViewport2 = ViewportGetViewport2;
        table.SetViewport2 = ViewportSetViewport2;
        initialized = true;
    }
    return &table;
}

struct RootFacade
{
    IDirectDraw4 direct_draw = {DirectDrawVtable()};
    IDirect3D3 direct3d = {Direct3dVtable()};
    volatile LONG references = 1;
    DWORD magic = kRootMagic;
    DWORD width = 640;
    DWORD height = 480;
    DWORD bits_per_pixel = 16;
    HWND window = nullptr;
    re2dj::platform::windows::Direct3d3OpenGlBackend* render_backend = nullptr;
};

struct SurfaceFacade
{
    IDirectDrawSurface4 interface_value = {SurfaceVtable()};
    IDirect3DTexture2 texture_interface = {TextureVtable()};
    volatile LONG references = 1;
    DWORD magic = kSurfaceMagic;
    RootFacade* root = nullptr;
    SurfaceFacade* attached_back_buffer = nullptr;
    DWORD width = 640;
    DWORD height = 480;
    DWORD bits_per_pixel = 16;
    DWORD pitch = 0;
    DWORD capabilities = 0;
    HDC bitmap_dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous_bitmap = nullptr;
    void* pixels = nullptr;
    bool dc_acquired = false;
    bool has_source_blt_color_key = false;
    DDCOLORKEY source_blt_color_key = {};
};

struct DeviceFacade
{
    IDirect3DDevice3 interface_value = {DeviceVtable()};
    volatile LONG references = 1;
    DWORD magic = kDeviceMagic;
    RootFacade* root = nullptr;
    SurfaceFacade* render_target = nullptr;
    IDirect3DViewport3* attached_viewport = nullptr;
    IDirect3DViewport3* current_viewport = nullptr;
    IDirect3DTexture2* texture_stage_zero = nullptr;
    bool scene_active = false;
    std::array<DWORD, 256> render_states = {};
    std::array<DWORD, 256> light_states = {};
    std::array<std::array<DWORD, 64>, 8> texture_stage_states = {};
    std::array<D3DMATRIX, 32> transforms = {};
};

struct ViewportFacade
{
    IDirect3DViewport3 interface_value = {ViewportVtable()};
    volatile LONG references = 1;
    DWORD magic = kViewportMagic;
    RootFacade* root = nullptr;
    D3DVIEWPORT2 viewport = {};
};

RootFacade* RootFromDirectDraw(IDirectDraw4* self)
{
    return reinterpret_cast<RootFacade*>(reinterpret_cast<unsigned char*>(self) -
                                         offsetof(RootFacade, direct_draw));
}

RootFacade* RootFromDirect3d(IDirect3D3* self)
{
    return reinterpret_cast<RootFacade*>(reinterpret_cast<unsigned char*>(self) -
                                         offsetof(RootFacade, direct3d));
}

SurfaceFacade* SurfaceFromInterface(IDirectDrawSurface4* self)
{
    return reinterpret_cast<SurfaceFacade*>(reinterpret_cast<unsigned char*>(self) -
                                            offsetof(SurfaceFacade, interface_value));
}

SurfaceFacade* SurfaceFromTexture(IDirect3DTexture2* self)
{
    return reinterpret_cast<SurfaceFacade*>(reinterpret_cast<unsigned char*>(self) -
                                            offsetof(SurfaceFacade, texture_interface));
}

DeviceFacade* DeviceFromInterface(IDirect3DDevice3* self)
{
    return reinterpret_cast<DeviceFacade*>(reinterpret_cast<unsigned char*>(self) -
                                           offsetof(DeviceFacade, interface_value));
}

ViewportFacade* ViewportFromInterface(IDirect3DViewport3* self)
{
    return reinterpret_cast<ViewportFacade*>(reinterpret_cast<unsigned char*>(self) -
                                             offsetof(ViewportFacade, interface_value));
}

ULONG AddRootReference(RootFacade* root)
{
    return static_cast<ULONG>(InterlockedIncrement(&root->references));
}

ULONG ReleaseRootReference(RootFacade* root)
{
    const LONG references = InterlockedDecrement(&root->references);
    if (references == 0)
    {
        delete root->render_backend;
        root->magic = 0;
        delete root;
    }
    return static_cast<ULONG>(references);
}

void FillRgb565Format(DDPIXELFORMAT* format)
{
    std::memset(format, 0, sizeof(*format));
    format->dwSize = sizeof(*format);
    format->dwFlags = DDPF_RGB;
    format->dwRGBBitCount = 16;
    format->dwRBitMask = 0xf800;
    format->dwGBitMask = 0x07e0;
    format->dwBBitMask = 0x001f;
}

bool IsRgb565Format(const DDPIXELFORMAT& format)
{
    return format.dwSize == sizeof(DDPIXELFORMAT) &&
           (format.dwFlags & DDPF_RGB) != 0 && format.dwRGBBitCount == 16 &&
           format.dwRBitMask == 0xf800 && format.dwGBitMask == 0x07e0 &&
           format.dwBBitMask == 0x001f;
}

bool CreateRgb565GdiBacking(SurfaceFacade* surface)
{
    struct Rgb565BitmapInfo
    {
        BITMAPINFOHEADER header = {};
        DWORD masks[3] = {};
    } info;
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = static_cast<LONG>(surface->width);
    info.header.biHeight = -static_cast<LONG>(surface->height);
    info.header.biPlanes = 1;
    info.header.biBitCount = 16;
    info.header.biCompression = BI_BITFIELDS;
    info.masks[0] = 0xf800;
    info.masks[1] = 0x07e0;
    info.masks[2] = 0x001f;
    surface->pitch = (surface->width * 2 + 3) & ~DWORD{3};

    surface->bitmap_dc = CreateCompatibleDC(nullptr);
    if (surface->bitmap_dc == nullptr)
    {
        return false;
    }
    surface->bitmap = CreateDIBSection(surface->bitmap_dc,
                                       reinterpret_cast<BITMAPINFO*>(&info),
                                       DIB_RGB_COLORS,
                                       &surface->pixels,
                                       nullptr,
                                       0);
    if (surface->bitmap == nullptr || surface->pixels == nullptr)
    {
        DeleteDC(surface->bitmap_dc);
        surface->bitmap_dc = nullptr;
        return false;
    }
    surface->previous_bitmap = SelectObject(surface->bitmap_dc, surface->bitmap);
    if (surface->previous_bitmap == nullptr || surface->previous_bitmap == HGDI_ERROR)
    {
        DeleteObject(surface->bitmap);
        DeleteDC(surface->bitmap_dc);
        surface->bitmap = nullptr;
        surface->bitmap_dc = nullptr;
        surface->pixels = nullptr;
        surface->previous_bitmap = nullptr;
        return false;
    }
    return true;
}

void DestroyGdiBacking(SurfaceFacade* surface)
{
    if (surface->bitmap_dc != nullptr && surface->previous_bitmap != nullptr)
    {
        SelectObject(surface->bitmap_dc, surface->previous_bitmap);
    }
    if (surface->bitmap != nullptr)
    {
        DeleteObject(surface->bitmap);
    }
    if (surface->bitmap_dc != nullptr)
    {
        DeleteDC(surface->bitmap_dc);
    }
    surface->bitmap_dc = nullptr;
    surface->bitmap = nullptr;
    surface->previous_bitmap = nullptr;
    surface->pixels = nullptr;
    surface->dc_acquired = false;
}

HRESULT WINAPI RootQueryInterface(IDirectDraw4* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    RootFacade* const root = RootFromDirectDraw(self);
    if (root->magic != kRootMagic)
    {
        return E_FAIL;
    }
    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IDirectDraw) ||
        IsEqualGUID(iid, IID_IDirectDraw4))
    {
        *object = &root->direct_draw;
    }
    else if (IsEqualGUID(iid, IID_IDirect3D3))
    {
        *object = &root->direct3d;
    }
    else
    {
        return E_NOINTERFACE;
    }
    AddRootReference(root);
    return S_OK;
}

ULONG WINAPI RootAddRef(IDirectDraw4* self)
{
    return AddRootReference(RootFromDirectDraw(self));
}

ULONG WINAPI RootRelease(IDirectDraw4* self)
{
    return ReleaseRootReference(RootFromDirectDraw(self));
}

HRESULT WINAPI RootGetCaps(IDirectDraw4*, DDCAPS* driver_caps, DDCAPS* hel_caps)
{
    bool valid = true;
    const auto fill = [&valid](DDCAPS* caps) {
        if (caps != nullptr)
        {
            if (caps->dwSize != sizeof(DDCAPS))
            {
                valid = false;
                return;
            }
            const DWORD size = caps->dwSize;
            std::memset(caps, 0, size);
            caps->dwSize = size;
            caps->dwCaps = DDCAPS_3D;
        }
    };
    fill(driver_caps);
    fill(hel_caps);
    return (driver_caps == nullptr && hel_caps == nullptr) || !valid ? DDERR_INVALIDPARAMS
                                                                    : DD_OK;
}

HRESULT WINAPI RootCreateSurface(IDirectDraw4* self,
                                 DDSURFACEDESC2* descriptor,
                                 IDirectDrawSurface4** surface,
                                 IUnknown* outer)
{
    if (descriptor == nullptr || surface == nullptr || outer != nullptr ||
        descriptor->dwSize != sizeof(DDSURFACEDESC2))
    {
        return DDERR_INVALIDPARAMS;
    }
    *surface = nullptr;
    if ((descriptor->ddsCaps.dwCaps & DDSCAPS_TEXTURE) != 0)
    {
        constexpr DWORD kRequiredFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
                                         DDSD_PIXELFORMAT;
        if ((descriptor->dwFlags & kRequiredFlags) != kRequiredFlags ||
            descriptor->dwWidth == 0 || descriptor->dwHeight == 0 ||
            !IsRgb565Format(descriptor->ddpfPixelFormat))
        {
            return DDERR_INVALIDPIXELFORMAT;
        }
        auto* const texture = new (std::nothrow) SurfaceFacade;
        if (texture == nullptr)
        {
            return DDERR_OUTOFMEMORY;
        }
        RootFacade* const root = RootFromDirectDraw(self);
        texture->root = root;
        texture->width = descriptor->dwWidth;
        texture->height = descriptor->dwHeight;
        texture->bits_per_pixel = 16;
        texture->capabilities = descriptor->ddsCaps.dwCaps;
        if (!CreateRgb565GdiBacking(texture))
        {
            delete texture;
            return DDERR_OUTOFMEMORY;
        }
        AddRootReference(root);
        *surface = &texture->interface_value;
        OutputDebugStringA(kCreateTextureSurfaceMessage);
        return DD_OK;
    }
    if ((descriptor->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == 0 ||
        descriptor->dwBackBufferCount != 1)
    {
        return DDERR_UNSUPPORTED;
    }
    RootFacade* const root = RootFromDirectDraw(self);
    auto* const primary = new (std::nothrow) SurfaceFacade;
    auto* const back = new (std::nothrow) SurfaceFacade;
    if (primary == nullptr || back == nullptr)
    {
        delete primary;
        delete back;
        return DDERR_OUTOFMEMORY;
    }
    primary->root = root;
    primary->width = root->width;
    primary->height = root->height;
    primary->bits_per_pixel = root->bits_per_pixel;
    primary->capabilities = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
    primary->attached_back_buffer = back;
    back->root = root;
    back->width = root->width;
    back->height = root->height;
    back->bits_per_pixel = root->bits_per_pixel;
    back->capabilities = DDSCAPS_BACKBUFFER | DDSCAPS_3DDEVICE;
    if (!CreateRgb565GdiBacking(primary) || !CreateRgb565GdiBacking(back))
    {
        DestroyGdiBacking(primary);
        DestroyGdiBacking(back);
        delete primary;
        delete back;
        return DDERR_OUTOFMEMORY;
    }
    AddRootReference(root);
    AddRootReference(root);
    *surface = &primary->interface_value;
    return DD_OK;
}

HRESULT WINAPI RootSetCooperativeLevel(IDirectDraw4* self, HWND window, DWORD)
{
    if (window == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    RootFromDirectDraw(self)->window = window;
    return DD_OK;
}

HRESULT WINAPI RootSetDisplayMode(IDirectDraw4* self,
                                  DWORD width,
                                  DWORD height,
                                  DWORD bits_per_pixel,
                                  DWORD,
                                  DWORD)
{
    if (width != 640 || height != 480 || bits_per_pixel != 16)
    {
        return DDERR_UNSUPPORTEDMODE;
    }
    RootFacade* const root = RootFromDirectDraw(self);
    root->width = width;
    root->height = height;
    root->bits_per_pixel = bits_per_pixel;
    return DD_OK;
}

HRESULT WINAPI RootRestoreAllSurfaces(IDirectDraw4*)
{
    return DD_OK;
}

HRESULT WINAPI RootRestoreDisplayMode(IDirectDraw4*)
{
    return DD_OK;
}

HRESULT WINAPI D3dQueryInterface(IDirect3D3* self, REFIID iid, void** object)
{
    return RootQueryInterface(&RootFromDirect3d(self)->direct_draw, iid, object);
}

ULONG WINAPI D3dAddRef(IDirect3D3* self)
{
    return AddRootReference(RootFromDirect3d(self));
}

ULONG WINAPI D3dRelease(IDirect3D3* self)
{
    return ReleaseRootReference(RootFromDirect3d(self));
}

HRESULT WINAPI D3dCreateViewport(IDirect3D3* self,
                                 IDirect3DViewport3** viewport,
                                 IUnknown* outer)
{
    if (viewport == nullptr || outer != nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *viewport = nullptr;
    auto* const facade = new (std::nothrow) ViewportFacade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->root = RootFromDirect3d(self);
    facade->viewport.dwSize = sizeof(D3DVIEWPORT2);
    AddRootReference(facade->root);
    *viewport = &facade->interface_value;
    return DD_OK;
}

HRESULT WINAPI D3dFindDevice(IDirect3D3*,
                             D3DFINDDEVICESEARCH* search,
                             D3DFINDDEVICERESULT* result)
{
    OutputDebugStringA(kFindDeviceMessage);
    if (search == nullptr || result == nullptr ||
        search->dwSize != sizeof(D3DFINDDEVICESEARCH) ||
        result->dwSize != sizeof(D3DFINDDEVICERESULT))
    {
        return DDERR_INVALIDPARAMS;
    }
    if ((search->dwFlags & D3DFDS_HARDWARE) != 0 && search->bHardware == FALSE)
    {
        return DDERR_NOTFOUND;
    }
    std::memset(result, 0, sizeof(*result));
    result->dwSize = sizeof(*result);
    result->guid = IID_IDirect3DHALDevice;
    result->ddHwDesc.dwSize = sizeof(D3DDEVICEDESC);
    result->ddHwDesc.dwFlags = D3DDD_BCLIPPING | D3DDD_DEVICERENDERBITDEPTH |
                               D3DDD_DEVICEZBUFFERBITDEPTH;
    result->ddHwDesc.bClipping = TRUE;
    result->ddHwDesc.dwDeviceRenderBitDepth = DDBD_16;
    result->ddHwDesc.dwDeviceZBufferBitDepth = DDBD_16;
    return DD_OK;
}

HRESULT WINAPI D3dCreateDevice(IDirect3D3* self,
                               REFCLSID device_class,
                               IDirectDrawSurface4* render_target,
                               IDirect3DDevice3** device,
                               IUnknown* outer)
{
    OutputDebugStringA(kCreateDeviceMessage);
    if (device == nullptr || render_target == nullptr || outer != nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *device = nullptr;
    if (!IsEqualGUID(device_class, IID_IDirect3DHALDevice))
    {
        return DDERR_INVALIDOBJECT;
    }
    SurfaceFacade* const target = SurfaceFromInterface(render_target);
    if (target->magic != kSurfaceMagic || (target->capabilities & DDSCAPS_3DDEVICE) == 0)
    {
        return DDERR_INVALIDOBJECT;
    }
    auto* const facade = new (std::nothrow) DeviceFacade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->root = RootFromDirect3d(self);
    facade->render_target = target;
    AddRootReference(facade->root);
    SurfaceAddRef(render_target);
    *device = &facade->interface_value;
    return DD_OK;
}

HRESULT WINAPI D3dEnumZBufferFormats(IDirect3D3*,
                                     REFCLSID device_class,
                                     LPD3DENUMPIXELFORMATSCALLBACK callback,
                                     void* context)
{
    if (!IsEqualGUID(device_class, IID_IDirect3DHALDevice) || callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DDPIXELFORMAT format = {};
    format.dwSize = sizeof(format);
    format.dwFlags = DDPF_ZBUFFER;
    format.dwZBufferBitDepth = 16;
    callback(&format, context);
    return DD_OK;
}

HRESULT WINAPI SurfaceQueryInterface(IDirectDrawSurface4* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    if (surface->magic != kSurfaceMagic)
    {
        return E_FAIL;
    }
    if (IsEqualGUID(iid, IID_IDirect3DTexture2) &&
        (surface->capabilities & DDSCAPS_TEXTURE) != 0)
    {
        *object = &surface->texture_interface;
        SurfaceAddRef(self);
        return S_OK;
    }
    if (!IsEqualGUID(iid, IID_IUnknown) && !IsEqualGUID(iid, IID_IDirectDrawSurface4))
    {
        return E_NOINTERFACE;
    }
    *object = self;
    SurfaceAddRef(self);
    return S_OK;
}

ULONG WINAPI SurfaceAddRef(IDirectDrawSurface4* self)
{
    return static_cast<ULONG>(InterlockedIncrement(&SurfaceFromInterface(self)->references));
}

ULONG WINAPI SurfaceRelease(IDirectDrawSurface4* self)
{
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    const LONG references = InterlockedDecrement(&surface->references);
    if (references == 0)
    {
        SurfaceFacade* const attached = surface->attached_back_buffer;
        RootFacade* const root = surface->root;
        DestroyGdiBacking(surface);
        surface->magic = 0;
        delete surface;
        if (attached != nullptr)
        {
            SurfaceRelease(&attached->interface_value);
        }
        ReleaseRootReference(root);
    }
    return static_cast<ULONG>(references);
}

HRESULT WINAPI SurfaceBlt(IDirectDrawSurface4* self,
                          RECT* destination,
                          IDirectDrawSurface4* source,
                          RECT* source_rectangle,
                          DWORD flags,
                          DDBLTFX* effects)
{
    if (source != nullptr || source_rectangle != nullptr || flags != DDBLT_COLORFILL)
    {
        return DDERR_UNSUPPORTED;
    }
    if (effects == nullptr || effects->dwSize != sizeof(DDBLTFX))
    {
        return DDERR_INVALIDPARAMS;
    }
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    if (surface->pixels == nullptr || surface->dc_acquired)
    {
        return DDERR_SURFACEBUSY;
    }
    const RECT full = {0,
                       0,
                       static_cast<LONG>(surface->width),
                       static_cast<LONG>(surface->height)};
    const RECT& rectangle = destination != nullptr ? *destination : full;
    if (rectangle.left < 0 || rectangle.top < 0 || rectangle.right <= rectangle.left ||
        rectangle.bottom <= rectangle.top ||
        rectangle.right > static_cast<LONG>(surface->width) ||
        rectangle.bottom > static_cast<LONG>(surface->height))
    {
        return DDERR_INVALIDRECT;
    }
    const std::uint16_t color = static_cast<std::uint16_t>(effects->dwFillColor);
    auto* const pixels = static_cast<unsigned char*>(surface->pixels);
    for (LONG y = rectangle.top; y < rectangle.bottom; ++y)
    {
        auto* const row = reinterpret_cast<std::uint16_t*>(pixels + y * surface->pitch);
        std::fill(row + rectangle.left, row + rectangle.right, color);
    }
    return DD_OK;
}

HRESULT WINAPI SurfaceFlip(IDirectDrawSurface4* self,
                           IDirectDrawSurface4* override_surface,
                           DWORD)
{
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    if (surface->attached_back_buffer == nullptr ||
        (override_surface != nullptr &&
         override_surface != &surface->attached_back_buffer->interface_value))
    {
        return DDERR_NOTFLIPPABLE;
    }
    if (surface->root->render_backend != nullptr)
    {
        std::string error;
        if (!surface->root->render_backend->Present(&error))
        {
            OutputDebugStringA(kOpenGlFailureMessage);
            return DDERR_GENERIC;
        }
    }
    return DD_OK;
}

HRESULT WINAPI SurfaceGetAttachedSurface(IDirectDrawSurface4* self,
                                         DDSCAPS2* capabilities,
                                         IDirectDrawSurface4** surface)
{
    if (capabilities == nullptr || surface == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *surface = nullptr;
    SurfaceFacade* const facade = SurfaceFromInterface(self);
    if ((capabilities->dwCaps & DDSCAPS_BACKBUFFER) == 0 ||
        facade->attached_back_buffer == nullptr)
    {
        return DDERR_NOTFOUND;
    }
    *surface = &facade->attached_back_buffer->interface_value;
    SurfaceAddRef(*surface);
    return DD_OK;
}

HRESULT WINAPI SurfaceGetCaps(IDirectDrawSurface4* self, DDSCAPS2* capabilities)
{
    if (capabilities == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    std::memset(capabilities, 0, sizeof(*capabilities));
    capabilities->dwCaps = SurfaceFromInterface(self)->capabilities;
    return DD_OK;
}

HRESULT WINAPI SurfaceGetPixelFormat(IDirectDrawSurface4*, DDPIXELFORMAT* format)
{
    if (format == nullptr || format->dwSize != sizeof(DDPIXELFORMAT))
    {
        return DDERR_INVALIDPARAMS;
    }
    FillRgb565Format(format);
    return DD_OK;
}

HRESULT WINAPI SurfaceGetSurfaceDesc(IDirectDrawSurface4* self, DDSURFACEDESC2* descriptor)
{
    if (descriptor == nullptr || descriptor->dwSize != sizeof(DDSURFACEDESC2))
    {
        return DDERR_INVALIDPARAMS;
    }
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    std::memset(descriptor, 0, sizeof(*descriptor));
    descriptor->dwSize = sizeof(*descriptor);
    descriptor->dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT |
                          DDSD_PITCH;
    descriptor->dwHeight = surface->height;
    descriptor->dwWidth = surface->width;
    descriptor->lPitch = static_cast<LONG>(surface->pitch);
    descriptor->ddsCaps.dwCaps = surface->capabilities;
    FillRgb565Format(&descriptor->ddpfPixelFormat);
    return DD_OK;
}

HRESULT WINAPI SurfaceGetDC(IDirectDrawSurface4* self, HDC* dc)
{
    if (dc == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *dc = nullptr;
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    if (surface->bitmap_dc == nullptr)
    {
        return DDERR_UNSUPPORTED;
    }
    if (surface->dc_acquired)
    {
        return DDERR_DCALREADYCREATED;
    }
    surface->dc_acquired = true;
    *dc = surface->bitmap_dc;
    return DD_OK;
}

HRESULT WINAPI SurfaceIsLost(IDirectDrawSurface4*)
{
    return DD_OK;
}

HRESULT WINAPI SurfaceReleaseDC(IDirectDrawSurface4* self, HDC dc)
{
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    if (!surface->dc_acquired || dc == nullptr || dc != surface->bitmap_dc)
    {
        return DDERR_INVALIDPARAMS;
    }
    surface->dc_acquired = false;
    return DD_OK;
}

HRESULT WINAPI SurfaceRestore(IDirectDrawSurface4*)
{
    return DD_OK;
}

HRESULT WINAPI SurfaceSetColorKey(IDirectDrawSurface4* self,
                                  DWORD flags,
                                  DDCOLORKEY* color_key)
{
    if (flags != DDCKEY_SRCBLT || color_key == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    surface->source_blt_color_key = *color_key;
    surface->has_source_blt_color_key = true;
    return DD_OK;
}

HRESULT WINAPI TextureQueryInterface(IDirect3DTexture2* self, REFIID iid, void** object)
{
    return SurfaceQueryInterface(&SurfaceFromTexture(self)->interface_value, iid, object);
}

ULONG WINAPI TextureAddRef(IDirect3DTexture2* self)
{
    return SurfaceAddRef(&SurfaceFromTexture(self)->interface_value);
}

ULONG WINAPI TextureRelease(IDirect3DTexture2* self)
{
    return SurfaceRelease(&SurfaceFromTexture(self)->interface_value);
}

HRESULT WINAPI TextureGetHandle(IDirect3DTexture2*,
                                IDirect3DDevice2*,
                                D3DTEXTUREHANDLE*)
{
    return DDERR_UNSUPPORTED;
}

HRESULT WINAPI TexturePaletteChanged(IDirect3DTexture2*, DWORD, DWORD)
{
    return DDERR_UNSUPPORTED;
}

HRESULT WINAPI TextureLoad(IDirect3DTexture2*, IDirect3DTexture2*)
{
    return DDERR_UNSUPPORTED;
}

HRESULT WINAPI DeviceQueryInterface(IDirect3DDevice3* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    DeviceFacade* const device = DeviceFromInterface(self);
    if (device->magic != kDeviceMagic)
    {
        return E_FAIL;
    }
    if (!IsEqualGUID(iid, IID_IUnknown) && !IsEqualGUID(iid, IID_IDirect3DDevice3))
    {
        return E_NOINTERFACE;
    }
    *object = self;
    DeviceAddRef(self);
    return S_OK;
}

ULONG WINAPI DeviceAddRef(IDirect3DDevice3* self)
{
    return static_cast<ULONG>(InterlockedIncrement(&DeviceFromInterface(self)->references));
}

ULONG WINAPI DeviceRelease(IDirect3DDevice3* self)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    const LONG references = InterlockedDecrement(&device->references);
    if (references == 0)
    {
        if (device->current_viewport != nullptr)
        {
            ViewportRelease(device->current_viewport);
        }
        if (device->attached_viewport != nullptr)
        {
            ViewportRelease(device->attached_viewport);
        }
        if (device->texture_stage_zero != nullptr)
        {
            TextureRelease(device->texture_stage_zero);
        }
        SurfaceRelease(&device->render_target->interface_value);
        RootFacade* const root = device->root;
        device->magic = 0;
        delete device;
        ReleaseRootReference(root);
    }
    return static_cast<ULONG>(references);
}

HRESULT WINAPI DeviceGetCaps(IDirect3DDevice3*,
                             D3DDEVICEDESC* hardware,
                             D3DDEVICEDESC* software)
{
    if (hardware == nullptr || hardware->dwSize != sizeof(D3DDEVICEDESC))
    {
        return DDERR_INVALIDPARAMS;
    }
    std::memset(hardware, 0, sizeof(*hardware));
    hardware->dwSize = sizeof(*hardware);
    hardware->dwFlags = D3DDD_BCLIPPING | D3DDD_DEVICERENDERBITDEPTH |
                        D3DDD_DEVICEZBUFFERBITDEPTH;
    hardware->bClipping = TRUE;
    hardware->dwDeviceRenderBitDepth = DDBD_16;
    hardware->dwDeviceZBufferBitDepth = DDBD_16;
    if (software != nullptr)
    {
        if (software->dwSize != sizeof(D3DDEVICEDESC))
        {
            return DDERR_INVALIDPARAMS;
        }
        std::memset(software, 0, sizeof(*software));
        software->dwSize = sizeof(*software);
    }
    return DD_OK;
}

HRESULT WINAPI DeviceAddViewport(IDirect3DDevice3* self, IDirect3DViewport3* viewport)
{
    if (viewport == nullptr || ViewportFromInterface(viewport)->magic != kViewportMagic)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    if (device->attached_viewport != nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    ViewportAddRef(viewport);
    device->attached_viewport = viewport;
    return DD_OK;
}

HRESULT WINAPI DeviceDeleteViewport(IDirect3DDevice3* self, IDirect3DViewport3* viewport)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    if (viewport == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    if (device->attached_viewport != viewport)
    {
        return DDERR_NOTFOUND;
    }
    if (device->current_viewport == viewport)
    {
        ViewportRelease(device->current_viewport);
        device->current_viewport = nullptr;
    }
    ViewportRelease(device->attached_viewport);
    device->attached_viewport = nullptr;
    return DD_OK;
}

HRESULT WINAPI DeviceBeginScene(IDirect3DDevice3* self)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    if (device->scene_active)
    {
        return D3DERR_SCENE_IN_SCENE;
    }
    device->scene_active = true;
    return DD_OK;
}

HRESULT WINAPI DeviceEndScene(IDirect3DDevice3* self)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    if (!device->scene_active)
    {
        return D3DERR_SCENE_NOT_IN_SCENE;
    }
    device->scene_active = false;
    return DD_OK;
}

HRESULT WINAPI DeviceEnumTextureFormats(IDirect3DDevice3*,
                                        LPD3DENUMPIXELFORMATSCALLBACK callback,
                                        void* context)
{
    if (callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DDPIXELFORMAT format = {};
    FillRgb565Format(&format);
    callback(&format, context);
    return DD_OK;
}

HRESULT WINAPI DeviceGetDirect3D(IDirect3DDevice3* self, IDirect3D3** direct3d)
{
    if (direct3d == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    *direct3d = &device->root->direct3d;
    AddRootReference(device->root);
    return DD_OK;
}

HRESULT WINAPI DeviceSetCurrentViewport(IDirect3DDevice3* self, IDirect3DViewport3* viewport)
{
    if (viewport == nullptr || ViewportFromInterface(viewport)->magic != kViewportMagic)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    ViewportAddRef(viewport);
    if (device->current_viewport != nullptr)
    {
        ViewportRelease(device->current_viewport);
    }
    device->current_viewport = viewport;
    return DD_OK;
}

HRESULT WINAPI DeviceGetCurrentViewport(IDirect3DDevice3* self, IDirect3DViewport3** viewport)
{
    if (viewport == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    *viewport = device->current_viewport;
    if (*viewport == nullptr)
    {
        return DDERR_NOTFOUND;
    }
    ViewportAddRef(*viewport);
    return DD_OK;
}

HRESULT WINAPI DeviceGetRenderState(IDirect3DDevice3* self,
                                    D3DRENDERSTATETYPE state,
                                    DWORD* value)
{
    if (value == nullptr || static_cast<unsigned>(state) >= 256)
    {
        return DDERR_INVALIDPARAMS;
    }
    *value = DeviceFromInterface(self)->render_states[static_cast<unsigned>(state)];
    return DD_OK;
}

HRESULT WINAPI DeviceSetRenderState(IDirect3DDevice3* self,
                                    D3DRENDERSTATETYPE state,
                                    DWORD value)
{
    if (static_cast<unsigned>(state) >= 256)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFromInterface(self)->render_states[static_cast<unsigned>(state)] = value;
    return DD_OK;
}

HRESULT WINAPI DeviceGetLightState(IDirect3DDevice3* self,
                                   D3DLIGHTSTATETYPE state,
                                   DWORD* value)
{
    if (value == nullptr || static_cast<unsigned>(state) >= 256)
    {
        return DDERR_INVALIDPARAMS;
    }
    *value = DeviceFromInterface(self)->light_states[static_cast<unsigned>(state)];
    return DD_OK;
}

HRESULT WINAPI DeviceSetLightState(IDirect3DDevice3* self,
                                   D3DLIGHTSTATETYPE state,
                                   DWORD value)
{
    if (static_cast<unsigned>(state) >= 256)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFromInterface(self)->light_states[static_cast<unsigned>(state)] = value;
    return DD_OK;
}

HRESULT WINAPI DeviceSetTransform(IDirect3DDevice3* self,
                                  D3DTRANSFORMSTATETYPE state,
                                  D3DMATRIX* matrix)
{
    if (matrix == nullptr || static_cast<unsigned>(state) >= 32)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFromInterface(self)->transforms[static_cast<unsigned>(state)] = *matrix;
    return DD_OK;
}

HRESULT WINAPI DeviceGetTransform(IDirect3DDevice3* self,
                                  D3DTRANSFORMSTATETYPE state,
                                  D3DMATRIX* matrix)
{
    if (matrix == nullptr || static_cast<unsigned>(state) >= 32)
    {
        return DDERR_INVALIDPARAMS;
    }
    *matrix = DeviceFromInterface(self)->transforms[static_cast<unsigned>(state)];
    return DD_OK;
}

HRESULT WINAPI DeviceGetTexture(IDirect3DDevice3* self,
                                DWORD stage,
                                IDirect3DTexture2** texture)
{
    if (texture == nullptr || stage != 0)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    *texture = device->texture_stage_zero;
    if (*texture != nullptr)
    {
        TextureAddRef(*texture);
    }
    return DD_OK;
}

HRESULT WINAPI DeviceSetTexture(IDirect3DDevice3* self,
                                DWORD stage,
                                IDirect3DTexture2* texture)
{
    if (stage != 0)
    {
        return DDERR_UNSUPPORTED;
    }
    if (texture != nullptr)
    {
        SurfaceFacade* const surface = SurfaceFromTexture(texture);
        if (IsBadReadPtr(surface, sizeof(*surface)) != FALSE ||
            surface->magic != kSurfaceMagic ||
            (surface->capabilities & DDSCAPS_TEXTURE) == 0)
        {
            return DDERR_INVALIDOBJECT;
        }
        TextureAddRef(texture);
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    if (device->texture_stage_zero != nullptr)
    {
        TextureRelease(device->texture_stage_zero);
    }
    device->texture_stage_zero = texture;
    return DD_OK;
}

HRESULT WINAPI DeviceGetTextureStageState(IDirect3DDevice3* self,
                                          DWORD stage,
                                          D3DTEXTURESTAGESTATETYPE state,
                                          DWORD* value)
{
    const unsigned state_index = static_cast<unsigned>(state);
    if (value == nullptr || stage >= 8 || state_index >= 64)
    {
        return DDERR_INVALIDPARAMS;
    }
    *value = DeviceFromInterface(self)->texture_stage_states[stage][state_index];
    return DD_OK;
}

HRESULT WINAPI DeviceSetTextureStageState(IDirect3DDevice3* self,
                                          DWORD stage,
                                          D3DTEXTURESTAGESTATETYPE state,
                                          DWORD value)
{
    const unsigned state_index = static_cast<unsigned>(state);
    if (stage >= 8 || state_index >= 64)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFromInterface(self)->texture_stage_states[stage][state_index] = value;
    return DD_OK;
}

HRESULT WINAPI DeviceDrawPrimitive(IDirect3DDevice3* self,
                                   D3DPRIMITIVETYPE primitive,
                                   DWORD vertex_type,
                                   void* vertices,
                                   DWORD vertex_count,
                                   DWORD flags)
{
    if (primitive != D3DPT_TRIANGLESTRIP || vertex_type != D3DFVF_TLVERTEX ||
        vertices == nullptr || vertex_count < 3 || flags != 0 ||
        vertex_count > (std::numeric_limits<DWORD>::max)() /
                           re2dj::graphics::kTransformedLitVertexStride)
    {
        return DDERR_UNSUPPORTED;
    }
    const std::size_t bytes = static_cast<std::size_t>(vertex_count) *
                              re2dj::graphics::kTransformedLitVertexStride;
    if (IsBadReadPtr(vertices, bytes) != FALSE)
    {
        return DDERR_INVALIDPARAMS;
    }
    re2dj::graphics::LegacyDrawCommand command;
    std::string error;
    if (!re2dj::graphics::DecodeTransformedLitVertices(
            std::span<const std::byte>(static_cast<const std::byte*>(vertices), bytes),
            vertex_count,
            &command,
            &error))
    {
        return DDERR_INVALIDPARAMS;
    }

    DeviceFacade* const device = DeviceFromInterface(self);
    RootFacade* const root = device->root;
    if (root->window == nullptr)
    {
        return DDERR_NOCOOPERATIVELEVELSET;
    }
    if (root->render_backend == nullptr)
    {
        auto* const backend = new (std::nothrow)
            re2dj::platform::windows::Direct3d3OpenGlBackend;
        if (backend == nullptr || !backend->Initialize(root->window, &error))
        {
            delete backend;
            OutputDebugStringA(kOpenGlFailureMessage);
            return DDERR_GENERIC;
        }
        root->render_backend = backend;
    }

    re2dj::platform::windows::Rgb565TextureView texture_view;
    const re2dj::platform::windows::Rgb565TextureView* texture = nullptr;
    if (device->texture_stage_zero != nullptr)
    {
        const SurfaceFacade* const surface = SurfaceFromTexture(device->texture_stage_zero);
        texture_view.pixels = surface->pixels;
        texture_view.width = surface->width;
        texture_view.height = surface->height;
        texture_view.pitch = surface->pitch;
        texture_view.has_source_color_key = surface->has_source_blt_color_key;
        texture_view.source_color_key =
            static_cast<std::uint16_t>(surface->source_blt_color_key.dwColorSpaceLowValue);
        texture = &texture_view;
    }
    if (!root->render_backend->Draw(command,
                                    root->width,
                                    root->height,
                                    texture,
                                    &error))
    {
        OutputDebugStringA(kOpenGlFailureMessage);
        return DDERR_GENERIC;
    }
    OutputDebugStringA(kDrawPrimitiveMessage);
    return DD_OK;
}

HRESULT WINAPI ViewportQueryInterface(IDirect3DViewport3* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    ViewportFacade* const viewport = ViewportFromInterface(self);
    if (viewport->magic != kViewportMagic)
    {
        return E_FAIL;
    }
    if (!IsEqualGUID(iid, IID_IUnknown) && !IsEqualGUID(iid, IID_IDirect3DViewport3))
    {
        return E_NOINTERFACE;
    }
    *object = self;
    ViewportAddRef(self);
    return S_OK;
}

ULONG WINAPI ViewportAddRef(IDirect3DViewport3* self)
{
    return static_cast<ULONG>(InterlockedIncrement(&ViewportFromInterface(self)->references));
}

ULONG WINAPI ViewportRelease(IDirect3DViewport3* self)
{
    ViewportFacade* const viewport = ViewportFromInterface(self);
    const LONG references = InterlockedDecrement(&viewport->references);
    if (references == 0)
    {
        RootFacade* const root = viewport->root;
        viewport->magic = 0;
        delete viewport;
        ReleaseRootReference(root);
    }
    return static_cast<ULONG>(references);
}

HRESULT WINAPI ViewportGetViewport2(IDirect3DViewport3* self, D3DVIEWPORT2* viewport)
{
    if (viewport == nullptr || viewport->dwSize != sizeof(D3DVIEWPORT2))
    {
        return DDERR_INVALIDPARAMS;
    }
    *viewport = ViewportFromInterface(self)->viewport;
    return DD_OK;
}

HRESULT WINAPI ViewportSetViewport2(IDirect3DViewport3* self, D3DVIEWPORT2* viewport)
{
    if (viewport == nullptr || viewport->dwSize != sizeof(D3DVIEWPORT2))
    {
        return DDERR_INVALIDPARAMS;
    }
    ViewportFromInterface(self)->viewport = *viewport;
    return DD_OK;
}

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI Re2djHleDirectDrawCreate(
    GUID*,
    LPDIRECTDRAW* direct_draw,
    IUnknown* outer)
{
    OutputDebugStringA(kDirectDrawCreateMessage);
    if (direct_draw == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *direct_draw = nullptr;
    if (outer != nullptr)
    {
        return CLASS_E_NOAGGREGATION;
    }
    auto* const facade = new (std::nothrow) RootFacade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    *direct_draw = reinterpret_cast<LPDIRECTDRAW>(&facade->direct_draw);
    return DD_OK;
}
