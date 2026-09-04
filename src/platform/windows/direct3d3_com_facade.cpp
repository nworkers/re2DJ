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
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>

#include "re2dj/graphics/legacy_draw_command.h"
#include "re2dj/graphics/legacy_texture.h"
#include "re2dj/graphics/legacy_transform.h"
#include "re2dj/graphics/legacy_vertex_buffer.h"
#include "re2dj/graphics/sdl3_opengl_backend.h"
#include "graphics_trace_log.h"
#include "window_mode.h"

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
constexpr DWORD kVertexBufferMagic = 0x52325642;

struct RootFacade;
struct SurfaceFacade;
struct DeviceFacade;
struct ViewportFacade;
struct VertexBufferFacade;

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
HRESULT WINAPI D3dCreateVertexBuffer(IDirect3D3* self,
                                     D3DVERTEXBUFFERDESC* descriptor,
                                     IDirect3DVertexBuffer** vertex_buffer,
                                     DWORD flags,
                                     IUnknown* outer);

HRESULT WINAPI VbQueryInterface(IDirect3DVertexBuffer* self, REFIID iid, void** object);
ULONG WINAPI VbAddRef(IDirect3DVertexBuffer* self);
ULONG WINAPI VbRelease(IDirect3DVertexBuffer* self);
HRESULT WINAPI VbLock(IDirect3DVertexBuffer* self, DWORD flags, void** data, DWORD* size);
HRESULT WINAPI VbUnlock(IDirect3DVertexBuffer* self);
HRESULT WINAPI VbProcessVertices(IDirect3DVertexBuffer* self,
                                 DWORD operation,
                                 DWORD destination_start,
                                 DWORD vertex_count,
                                 IDirect3DVertexBuffer* source,
                                 DWORD source_start,
                                 IDirect3DDevice3* device,
                                 DWORD flags);
HRESULT WINAPI VbGetVertexBufferDesc(IDirect3DVertexBuffer* self,
                                     D3DVERTEXBUFFERDESC* descriptor);
HRESULT WINAPI VbOptimize(IDirect3DVertexBuffer* self,
                          IDirect3DDevice3* device,
                          DWORD flags);

HRESULT WINAPI SurfaceQueryInterface(IDirectDrawSurface4* self, REFIID iid, void** object);
ULONG WINAPI SurfaceAddRef(IDirectDrawSurface4* self);
ULONG WINAPI SurfaceRelease(IDirectDrawSurface4* self);
HRESULT WINAPI SurfaceBlt(IDirectDrawSurface4* self,
                          RECT* destination,
                          IDirectDrawSurface4* source,
                          RECT* source_rectangle,
                          DWORD flags,
                          DDBLTFX* effects);
HRESULT WINAPI SurfaceBltFast(IDirectDrawSurface4* self,
                              DWORD destination_x,
                              DWORD destination_y,
                              IDirectDrawSurface4* source,
                              RECT* source_rectangle,
                              DWORD flags);
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
HRESULT WINAPI DeviceDrawIndexedPrimitiveVB(IDirect3DDevice3* self,
                                            D3DPRIMITIVETYPE primitive,
                                            IDirect3DVertexBuffer* vertex_buffer,
                                            WORD* indices,
                                            DWORD index_count,
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
        table.CreateVertexBuffer = D3dCreateVertexBuffer;
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
        table.BltFast = SurfaceBltFast;
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
        table.DrawIndexedPrimitiveVB = DeviceDrawIndexedPrimitiveVB;
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

IDirect3DVertexBufferVtbl* VertexBufferVtable()
{
    static IDirect3DVertexBufferVtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = VbQueryInterface;
        table.AddRef = VbAddRef;
        table.Release = VbRelease;
        table.Lock = VbLock;
        table.Unlock = VbUnlock;
        table.ProcessVertices = VbProcessVertices;
        table.GetVertexBufferDesc = VbGetVertexBufferDesc;
        table.Optimize = VbOptimize;
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
    std::uint64_t next_texture_identity = 1;
    std::uint32_t next_surface_diagnostic_id = 1;
    std::uint64_t next_composition_diagnostic_sequence = 1;
    std::uint32_t create_surface_diagnostic_count = 0;
    std::uint32_t surface_dc_diagnostic_count = 0;
    std::uint32_t source_blt_diagnostic_count = 0;
    std::uint32_t source_blt_target_diagnostic_count = 0;
    std::uint32_t texture_load_diagnostic_count = 0;
    std::uint32_t color_fill_diagnostic_count = 0;
    std::uint32_t flip_diagnostic_count = 0;
    std::uint32_t draw_failure_diagnostic_count = 0;
    std::uint32_t untextured_draw_diagnostic_count = 0;
    std::uint32_t late_draw_diagnostic_count = 0;
    std::uint32_t late_draw_target_diagnostic_count = 0;
    std::uint32_t transform_diagnostic_count = 0;
    std::uint64_t frame_number = 0;
    LARGE_INTEGER fps_frequency = {};
    LARGE_INTEGER fps_interval_start = {};
    std::uint32_t fps_interval_frames = 0;
    re2dj::graphics::Sdl3OpenGlBackend* render_backend = nullptr;
};

void RecordPresentedFrame(RootFacade* root)
{
    LARGE_INTEGER now = {};
    if (root == nullptr || QueryPerformanceCounter(&now) == FALSE)
    {
        return;
    }
    if (root->fps_frequency.QuadPart == 0)
    {
        if (QueryPerformanceFrequency(&root->fps_frequency) == FALSE ||
            root->fps_frequency.QuadPart <= 0)
        {
            return;
        }
        root->fps_interval_start = now;
    }
    ++root->fps_interval_frames;
    const LONGLONG elapsed_ticks = now.QuadPart - root->fps_interval_start.QuadPart;
    if (elapsed_ticks < root->fps_frequency.QuadPart)
    {
        return;
    }
    const double fps = static_cast<double>(root->fps_interval_frames) *
                       static_cast<double>(root->fps_frequency.QuadPart) /
                       static_cast<double>(elapsed_ticks);
    Re2djUpdateWindowTitle(root->window, fps);
    root->fps_interval_start = now;
    root->fps_interval_frames = 0;
}

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
    std::uint32_t diagnostic_id = 0;
    std::uint64_t texture_identity = 0;
    std::uint64_t texture_revision = 1;
    bool dc_acquired = false;
    bool has_source_blt_color_key = false;
    bool draw_diagnostic_reported = false;
    bool content_diagnostic_computed = false;
    std::uint64_t diagnostic_non_key_pixels = 0;
    std::uint64_t diagnostic_nonzero_pixels = 0;
    bool diagnostic_non_key_bounds_valid = false;
    bool diagnostic_nonzero_bounds_valid = false;
    std::uint32_t diagnostic_non_key_min_x = 0;
    std::uint32_t diagnostic_non_key_min_y = 0;
    std::uint32_t diagnostic_non_key_max_x = 0;
    std::uint32_t diagnostic_non_key_max_y = 0;
    std::uint32_t diagnostic_nonzero_min_x = 0;
    std::uint32_t diagnostic_nonzero_min_y = 0;
    std::uint32_t diagnostic_nonzero_max_x = 0;
    std::uint32_t diagnostic_nonzero_max_y = 0;
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
    bool draw_success_reported = false;
    bool draw_failure_reported = false;
    std::array<DWORD, 256> render_states = {};
    std::array<std::uint8_t, 256> render_state_reports = {};
    std::array<DWORD, 256> light_states = {};
    std::array<std::array<DWORD, 64>, 8> texture_stage_states = {};
    std::array<std::array<std::uint8_t, 64>, 8> texture_stage_state_reports = {};
    std::array<D3DMATRIX, 32> transforms = {};
};

SurfaceFacade* SurfaceFromTexture(IDirect3DTexture2* self);
ViewportFacade* ViewportFromInterface(IDirect3DViewport3* self);

void MarkSurfaceDirty(SurfaceFacade* surface)
{
    ++surface->texture_revision;
    if (surface->texture_revision == 0)
    {
        surface->texture_revision = 1;
    }
    surface->content_diagnostic_computed = false;
    surface->diagnostic_non_key_pixels = 0;
    surface->diagnostic_nonzero_pixels = 0;
    surface->diagnostic_non_key_bounds_valid = false;
    surface->diagnostic_nonzero_bounds_valid = false;
    surface->diagnostic_non_key_min_x = 0;
    surface->diagnostic_non_key_min_y = 0;
    surface->diagnostic_non_key_max_x = 0;
    surface->diagnostic_non_key_max_y = 0;
    surface->diagnostic_nonzero_min_x = 0;
    surface->diagnostic_nonzero_min_y = 0;
    surface->diagnostic_nonzero_max_x = 0;
    surface->diagnostic_nonzero_max_y = 0;
}

std::uint64_t AllocateSurfaceIdentity(RootFacade* root)
{
    const std::uint64_t identity = root->next_texture_identity++;
    if (root->next_texture_identity == 0)
    {
        root->next_texture_identity = 1;
    }
    return identity;
}

std::uint32_t AllocateSurfaceDiagnosticId(RootFacade* root)
{
    const std::uint32_t id = root->next_surface_diagnostic_id++;
    if (root->next_surface_diagnostic_id == 0)
    {
        root->next_surface_diagnostic_id = 1;
    }
    return id;
}

void ReportCompositionDiagnostic(RootFacade* root, const char* detail)
{
    if (root == nullptr || detail == nullptr)
    {
        return;
    }
    const std::uint64_t sequence = root->next_composition_diagnostic_sequence++;
    re2dj::platform::windows::WriteGraphicsTraceFormat(
        "re2dj:hle:ddraw-trace:seq=%llu:%s",
        static_cast<unsigned long long>(sequence),
        detail);
}

void ReportCreateSurfaceDiagnostic(RootFacade* root,
                                   const DDSURFACEDESC2& descriptor,
                                   const SurfaceFacade* surface,
                                   HRESULT result)
{
    constexpr std::uint32_t kMaximumCreateSurfaceDiagnostics = 256;
    if (root == nullptr ||
        ++root->create_surface_diagnostic_count > kMaximumCreateSurfaceDiagnostics)
    {
        return;
    }
    char detail[320] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "CreateSurface:id=%lu:flags=0x%08lx:caps=0x%08lx:size=%lux%lu:result=0x%08lx",
                  surface == nullptr ? 0UL : static_cast<unsigned long>(surface->diagnostic_id),
                  static_cast<unsigned long>(descriptor.dwFlags),
                  static_cast<unsigned long>(descriptor.ddsCaps.dwCaps),
                  static_cast<unsigned long>(descriptor.dwWidth),
                  static_cast<unsigned long>(descriptor.dwHeight),
                  static_cast<unsigned long>(result));
    ReportCompositionDiagnostic(root, detail);
}

void ReportBltDiagnostic(const char* operation,
                         const SurfaceFacade* destination,
                         const RECT* destination_rectangle,
                         const SurfaceFacade* source,
                         const RECT* source_rectangle,
                         DWORD flags,
                         HRESULT result)
{
    if (destination == nullptr)
    {
        return;
    }
    constexpr std::uint64_t kTargetFrame = 3000;
    constexpr std::uint32_t kMaximumSourceBltDiagnostics = 256;
    constexpr std::uint32_t kMaximumSourceBltTargetDiagnostics = 2048;
    constexpr std::uint32_t kMaximumColorFillDiagnostics = 8;
    std::uint32_t* diagnostic_count = nullptr;
    std::uint32_t maximum_diagnostics = 0;
    if (source == nullptr)
    {
        diagnostic_count = &destination->root->color_fill_diagnostic_count;
        maximum_diagnostics = kMaximumColorFillDiagnostics;
    }
    else if (destination->root->frame_number >= kTargetFrame)
    {
        diagnostic_count = &destination->root->source_blt_target_diagnostic_count;
        maximum_diagnostics = kMaximumSourceBltTargetDiagnostics;
    }
    else
    {
        diagnostic_count = &destination->root->source_blt_diagnostic_count;
        maximum_diagnostics = kMaximumSourceBltDiagnostics;
    }
    if (++(*diagnostic_count) > maximum_diagnostics)
    {
        return;
    }
    const RECT destination_full = {
        0, 0, static_cast<LONG>(destination->width), static_cast<LONG>(destination->height)};
    const RECT source_full = source == nullptr
                                 ? RECT{0, 0, 0, 0}
                                 : RECT{0,
                                        0,
                                        static_cast<LONG>(source->width),
                                        static_cast<LONG>(source->height)};
    const RECT& destination_region =
        destination_rectangle == nullptr ? destination_full : *destination_rectangle;
    const RECT& source_region = source_rectangle == nullptr ? source_full : *source_rectangle;
    char detail[640] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "%s:frame=%llu:dst=%lu:dstsize=%lux%lu:dstcaps=0x%08lx:dstkey=%u:dstrect=%ld,%ld,%ld,%ld:src=%lu:srcsize=%lux%lu:srccaps=0x%08lx:srckey=%u:srcrect=%ld,%ld,%ld,%ld:flags=0x%08lx:result=0x%08lx",
                  operation,
                  static_cast<unsigned long long>(destination->root->frame_number),
                  static_cast<unsigned long>(destination->diagnostic_id),
                  static_cast<unsigned long>(destination->width),
                  static_cast<unsigned long>(destination->height),
                  static_cast<unsigned long>(destination->capabilities),
                  destination->has_source_blt_color_key ? 1U : 0U,
                  destination_region.left,
                  destination_region.top,
                  destination_region.right,
                  destination_region.bottom,
                  source == nullptr ? 0UL : static_cast<unsigned long>(source->diagnostic_id),
                  source == nullptr ? 0UL : static_cast<unsigned long>(source->width),
                  source == nullptr ? 0UL : static_cast<unsigned long>(source->height),
                  source == nullptr ? 0UL : static_cast<unsigned long>(source->capabilities),
                  source != nullptr && source->has_source_blt_color_key ? 1U : 0U,
                  source_region.left,
                  source_region.top,
                  source_region.right,
                  source_region.bottom,
                  static_cast<unsigned long>(flags),
                  static_cast<unsigned long>(result));
    ReportCompositionDiagnostic(destination->root, detail);
}

void ReportSurfaceDiagnostic(const char* operation,
                             const SurfaceFacade* surface,
                             HRESULT result)
{
    if (surface == nullptr)
    {
        return;
    }
    const bool is_flip = std::strcmp(operation, "Flip") == 0;
    constexpr std::uint32_t kMaximumSurfaceDcDiagnostics = 256;
    constexpr std::uint32_t kMaximumFlipDiagnostics = 8;
    std::uint32_t& diagnostic_count = is_flip ? surface->root->flip_diagnostic_count
                                              : surface->root->surface_dc_diagnostic_count;
    const std::uint32_t maximum_diagnostics =
        is_flip ? kMaximumFlipDiagnostics : kMaximumSurfaceDcDiagnostics;
    if (++diagnostic_count > maximum_diagnostics)
    {
        return;
    }
    char detail[160] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "%s:id=%lu:caps=0x%08lx:revision=%llu:result=0x%08lx",
                  operation,
                  static_cast<unsigned long>(surface->diagnostic_id),
                  static_cast<unsigned long>(surface->capabilities),
                  static_cast<unsigned long long>(surface->texture_revision),
                  static_cast<unsigned long>(result));
    ReportCompositionDiagnostic(surface->root, detail);
}

void ReportTextureLoadDiagnostic(const SurfaceFacade* destination,
                                 const SurfaceFacade* source,
                                 HRESULT result)
{
    if (destination == nullptr || destination->root == nullptr)
    {
        return;
    }
    constexpr std::uint32_t kMaximumTextureLoadDiagnostics = 256;
    if (++destination->root->texture_load_diagnostic_count >
        kMaximumTextureLoadDiagnostics)
    {
        return;
    }
    char detail[192] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "TextureLoad:dst=%lu:src=%lu:revision=%llu:result=0x%08lx",
                  static_cast<unsigned long>(destination->diagnostic_id),
                  source == nullptr ? 0UL : static_cast<unsigned long>(source->diagnostic_id),
                  static_cast<unsigned long long>(destination->texture_revision),
                  static_cast<unsigned long>(result));
    ReportCompositionDiagnostic(destination->root, detail);
}

void ReportDrawDiagnostic(DeviceFacade* device,
                          D3DPRIMITIVETYPE primitive,
                          DWORD vertex_type,
                          DWORD vertex_count,
                          DWORD flags,
                          HRESULT result,
                          const char* reason)
{
    if (device == nullptr || device->root == nullptr)
    {
        return;
    }
    SurfaceFacade* texture_surface = device->texture_stage_zero == nullptr
                                         ? nullptr
                                         : SurfaceFromTexture(device->texture_stage_zero);
    if (result == DD_OK && texture_surface != nullptr &&
        texture_surface->draw_diagnostic_reported)
    {
        return;
    }
    constexpr std::uint32_t kMaximumDrawFailureDiagnostics = 64;
    constexpr std::uint32_t kMaximumUntexturedDrawDiagnostics = 16;
    if (result != DD_OK &&
        ++device->root->draw_failure_diagnostic_count > kMaximumDrawFailureDiagnostics)
    {
        return;
    }
    if (result == DD_OK && texture_surface == nullptr &&
        ++device->root->untextured_draw_diagnostic_count > kMaximumUntexturedDrawDiagnostics)
    {
        return;
    }
    if (result == DD_OK && texture_surface != nullptr)
    {
        texture_surface->draw_diagnostic_reported = true;
    }
    const auto& stage = device->texture_stage_states[0];
    char detail[512] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "DrawPrimitive:texture=%lu:primitive=%lu:fvf=0x%08lx:vertices=%lu:flags=0x%08lx:result=0x%08lx:reason=%s:colorop=%lu:colorarg1=0x%08lx:colorarg2=0x%08lx:alphatest=%lu:alphafunc=%lu:blend=%lu:srcblend=%lu:dstblend=%lu:minfilter=%lu:magfilter=%lu",
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(texture_surface->diagnostic_id),
                  static_cast<unsigned long>(primitive),
                  static_cast<unsigned long>(vertex_type),
                  static_cast<unsigned long>(vertex_count),
                  static_cast<unsigned long>(flags),
                  static_cast<unsigned long>(result),
                  reason == nullptr ? "none" : reason,
                  static_cast<unsigned long>(stage[D3DTSS_COLOROP]),
                  static_cast<unsigned long>(stage[D3DTSS_COLORARG1]),
                  static_cast<unsigned long>(stage[D3DTSS_COLORARG2]),
                  static_cast<unsigned long>(
                      device->render_states[D3DRENDERSTATE_ALPHATESTENABLE]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_ALPHAFUNC]),
                  static_cast<unsigned long>(
                      device->render_states[D3DRENDERSTATE_ALPHABLENDENABLE]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_SRCBLEND]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_DESTBLEND]),
                  static_cast<unsigned long>(stage[D3DTSS_MINFILTER]),
                  static_cast<unsigned long>(stage[D3DTSS_MAGFILTER]));
    ReportCompositionDiagnostic(device->root, detail);
}

void ReportLateDrawDiagnostic(
    DeviceFacade* device,
    const re2dj::graphics::LegacyDrawCommand& command,
    const re2dj::graphics::LegacyFixedFunctionState& state,
    DWORD flags,
    DWORD vertex_type)
{
    if (device == nullptr || device->root == nullptr || command.vertices.empty())
    {
        return;
    }
    const SurfaceFacade* texture_surface = device->texture_stage_zero == nullptr
                                               ? nullptr
                                               : SurfaceFromTexture(device->texture_stage_zero);
    // Keep the trace bounded while retaining the later menu composition after
    // the high-volume attract/demo draws have filled the general budget.
    constexpr std::uint64_t kTargetFrame = 3000;
    constexpr std::uint32_t kMaximumLateDrawDiagnostics = 16384;
    constexpr std::uint32_t kMaximumLateDrawTargetDiagnostics = 4096;
    if (device->root->frame_number >= kTargetFrame)
    {
        if (++device->root->late_draw_target_diagnostic_count >
            kMaximumLateDrawTargetDiagnostics)
        {
            return;
        }
    }
    else if (++device->root->late_draw_diagnostic_count > kMaximumLateDrawDiagnostics)
    {
        return;
    }
    float minimum_x = command.vertices.front().x;
    float minimum_y = command.vertices.front().y;
    float minimum_z = command.vertices.front().z;
    float minimum_reciprocal_w = command.vertices.front().reciprocal_w;
    float maximum_x = minimum_x;
    float maximum_y = minimum_y;
    float maximum_z = minimum_z;
    float maximum_reciprocal_w = minimum_reciprocal_w;
    float minimum_u = command.vertices.front().texture_u;
    float minimum_v = command.vertices.front().texture_v;
    float maximum_u = minimum_u;
    float maximum_v = minimum_v;
    for (const re2dj::graphics::TransformedLitVertex& vertex : command.vertices)
    {
        minimum_x = (std::min)(minimum_x, vertex.x);
        minimum_y = (std::min)(minimum_y, vertex.y);
        minimum_z = (std::min)(minimum_z, vertex.z);
        minimum_reciprocal_w = (std::min)(minimum_reciprocal_w, vertex.reciprocal_w);
        maximum_x = (std::max)(maximum_x, vertex.x);
        maximum_y = (std::max)(maximum_y, vertex.y);
        maximum_z = (std::max)(maximum_z, vertex.z);
        maximum_reciprocal_w = (std::max)(maximum_reciprocal_w, vertex.reciprocal_w);
        minimum_u = (std::min)(minimum_u, vertex.texture_u);
        minimum_v = (std::min)(minimum_v, vertex.texture_v);
        maximum_u = (std::max)(maximum_u, vertex.texture_u);
        maximum_v = (std::max)(maximum_v, vertex.texture_v);
    }
    if (texture_surface != nullptr && !texture_surface->content_diagnostic_computed &&
        texture_surface->pixels != nullptr)
    {
        SurfaceFacade* mutable_surface = const_cast<SurfaceFacade*>(texture_surface);
        const auto* const pixels = static_cast<const unsigned char*>(texture_surface->pixels);
        for (std::uint32_t y = 0; y < texture_surface->height; ++y)
        {
            const auto* const row = reinterpret_cast<const std::uint16_t*>(
                pixels + static_cast<std::size_t>(y) * texture_surface->pitch);
            for (std::uint32_t x = 0; x < texture_surface->width; ++x)
            {
                const std::uint16_t pixel = row[x];
                if (pixel != 0)
                {
                    ++mutable_surface->diagnostic_nonzero_pixels;
                    if (!mutable_surface->diagnostic_nonzero_bounds_valid)
                    {
                        mutable_surface->diagnostic_nonzero_bounds_valid = true;
                        mutable_surface->diagnostic_nonzero_min_x = x;
                        mutable_surface->diagnostic_nonzero_min_y = y;
                        mutable_surface->diagnostic_nonzero_max_x = x;
                        mutable_surface->diagnostic_nonzero_max_y = y;
                    }
                    else
                    {
                        mutable_surface->diagnostic_nonzero_min_x =
                            (std::min)(mutable_surface->diagnostic_nonzero_min_x, x);
                        mutable_surface->diagnostic_nonzero_min_y =
                            (std::min)(mutable_surface->diagnostic_nonzero_min_y, y);
                        mutable_surface->diagnostic_nonzero_max_x =
                            (std::max)(mutable_surface->diagnostic_nonzero_max_x, x);
                        mutable_surface->diagnostic_nonzero_max_y =
                            (std::max)(mutable_surface->diagnostic_nonzero_max_y, y);
                    }
                }
                const bool matches_key = texture_surface->has_source_blt_color_key &&
                                         pixel >= texture_surface->source_blt_color_key
                                                      .dwColorSpaceLowValue &&
                                         pixel <= texture_surface->source_blt_color_key
                                                      .dwColorSpaceHighValue;
                if (!matches_key)
                {
                    ++mutable_surface->diagnostic_non_key_pixels;
                    if (!mutable_surface->diagnostic_non_key_bounds_valid)
                    {
                        mutable_surface->diagnostic_non_key_bounds_valid = true;
                        mutable_surface->diagnostic_non_key_min_x = x;
                        mutable_surface->diagnostic_non_key_min_y = y;
                        mutable_surface->diagnostic_non_key_max_x = x;
                        mutable_surface->diagnostic_non_key_max_y = y;
                    }
                    else
                    {
                        mutable_surface->diagnostic_non_key_min_x =
                            (std::min)(mutable_surface->diagnostic_non_key_min_x, x);
                        mutable_surface->diagnostic_non_key_min_y =
                            (std::min)(mutable_surface->diagnostic_non_key_min_y, y);
                        mutable_surface->diagnostic_non_key_max_x =
                            (std::max)(mutable_surface->diagnostic_non_key_max_x, x);
                        mutable_surface->diagnostic_non_key_max_y =
                            (std::max)(mutable_surface->diagnostic_non_key_max_y, y);
                    }
                }
            }
        }
        mutable_surface->content_diagnostic_computed = true;
    }
    char detail[960] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "LateDraw:frame=%llu:fvf=0x%08lx:texture=%lu:topology=%u:vertices=%lu:bounds=%.3f,%.3f,%.3f,%.3f:z=%.6f,%.6f:rhw=%.6f,%.6f:uv=%.6f,%.6f,%.6f,%.6f:diffuse=0x%08lx:flags=0x%08lx:blend=%lu:srcblend=%lu:dstblend=%lu:zenable=%lu:zwrite=%lu:zfunc=%lu:texsize=%lux%lu:key=%u:colorkey=%lu:alphatest=%lu:keylow=0x%04lx:keyhigh=0x%04lx:nonkey=%llu:nonzero=%llu:nonkeybbox=%lu,%lu,%lu,%lu:nonzerobbox=%lu,%lu,%lu,%lu:effectiveblend=%u:effectivesrcblend=%u:effectivedstblend=%u:effectivedepth=%u:%u:%u:fadecompat=%u",
                  static_cast<unsigned long long>(device->root->frame_number),
                  static_cast<unsigned long>(vertex_type),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(texture_surface->diagnostic_id),
                  command.topology == re2dj::graphics::PrimitiveTopology::kLineList
                      ? 2U
                      : command.topology == re2dj::graphics::PrimitiveTopology::kTriangleList
                            ? 4U
                            : 5U,
                  static_cast<unsigned long>(command.vertices.size()),
                  minimum_x,
                  minimum_y,
                  maximum_x,
                  maximum_y,
                  minimum_z,
                  maximum_z,
                  minimum_reciprocal_w,
                  maximum_reciprocal_w,
                  minimum_u,
                  minimum_v,
                  maximum_u,
                  maximum_v,
                  static_cast<unsigned long>(command.vertices.front().diffuse_argb),
                  static_cast<unsigned long>(flags),
                  static_cast<unsigned long>(
                      device->render_states[D3DRENDERSTATE_ALPHABLENDENABLE]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_SRCBLEND]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_DESTBLEND]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_ZENABLE]),
                  static_cast<unsigned long>(
                      device->render_states[D3DRENDERSTATE_ZWRITEENABLE]),
                  static_cast<unsigned long>(device->render_states[D3DRENDERSTATE_ZFUNC]),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(texture_surface->width),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(texture_surface->height),
                  texture_surface != nullptr && texture_surface->has_source_blt_color_key ? 1U
                                                                                          : 0U,
                  static_cast<unsigned long>(
                      device->render_states[D3DRENDERSTATE_COLORKEYENABLE]),
                  static_cast<unsigned long>(
                      device->render_states[D3DRENDERSTATE_ALPHATESTENABLE]),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(
                            texture_surface->source_blt_color_key.dwColorSpaceLowValue),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(
                            texture_surface->source_blt_color_key.dwColorSpaceHighValue),
                  texture_surface == nullptr
                      ? 0ULL
                      : static_cast<unsigned long long>(
                            texture_surface->diagnostic_non_key_pixels),
                  texture_surface == nullptr
                      ? 0ULL
                      : static_cast<unsigned long long>(
                            texture_surface->diagnostic_nonzero_pixels),
                  texture_surface != nullptr && texture_surface->diagnostic_non_key_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_non_key_min_x)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_non_key_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_non_key_min_y)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_non_key_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_non_key_max_x)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_non_key_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_non_key_max_y)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_nonzero_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_nonzero_min_x)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_nonzero_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_nonzero_min_y)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_nonzero_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_nonzero_max_x)
                      : 0UL,
                  texture_surface != nullptr && texture_surface->diagnostic_nonzero_bounds_valid
                      ? static_cast<unsigned long>(texture_surface->diagnostic_nonzero_max_y)
                      : 0UL,
                  static_cast<unsigned>(state.alpha_blend_enabled ? 1 : 0),
                  static_cast<unsigned>(state.source_blend),
                  static_cast<unsigned>(state.destination_blend),
                  static_cast<unsigned>(state.depth_test_enabled ? 1 : 0),
                  static_cast<unsigned>(state.depth_write_enabled ? 1 : 0),
                  static_cast<unsigned>(state.depth_function),
                  static_cast<unsigned>(state.fade_compatibility_applied ? 1 : 0));
    ReportCompositionDiagnostic(device->root, detail);
}

bool BuildSurfaceRectangle(const SurfaceFacade& surface,
                           const RECT* input,
                           re2dj::graphics::Rgb565Rectangle* output)
{
    if (output == nullptr)
    {
        return false;
    }
    const RECT rectangle = input != nullptr
                               ? *input
                               : RECT{0,
                                      0,
                                      static_cast<LONG>(surface.width),
                                      static_cast<LONG>(surface.height)};
    if (rectangle.left < 0 || rectangle.top < 0 || rectangle.right <= rectangle.left ||
        rectangle.bottom <= rectangle.top ||
        rectangle.right > static_cast<LONG>(surface.width) ||
        rectangle.bottom > static_cast<LONG>(surface.height))
    {
        return false;
    }
    output->x = static_cast<std::uint32_t>(rectangle.left);
    output->y = static_cast<std::uint32_t>(rectangle.top);
    output->width = static_cast<std::uint32_t>(rectangle.right - rectangle.left);
    output->height = static_cast<std::uint32_t>(rectangle.bottom - rectangle.top);
    return true;
}

HRESULT CopySurfaceRectangle(SurfaceFacade* destination,
                             const re2dj::graphics::Rgb565Rectangle& destination_rectangle,
                             SurfaceFacade* source,
                             const re2dj::graphics::Rgb565Rectangle& source_rectangle,
                             bool use_source_color_key)
{
    if (destination == nullptr || source == nullptr || destination->magic != kSurfaceMagic ||
        source->magic != kSurfaceMagic || destination->pixels == nullptr ||
        source->pixels == nullptr || destination->dc_acquired || source->dc_acquired)
    {
        return DDERR_SURFACEBUSY;
    }
    if (destination_rectangle.width != source_rectangle.width ||
        destination_rectangle.height != source_rectangle.height)
    {
        return DDERR_UNSUPPORTED;
    }
    if (use_source_color_key && !source->has_source_blt_color_key)
    {
        return DDERR_NOCOLORKEY;
    }

    re2dj::graphics::Rgb565ColorKey key;
    if (use_source_color_key)
    {
        key.enabled = true;
        key.low = static_cast<std::uint16_t>(source->source_blt_color_key.dwColorSpaceLowValue);
        key.high = static_cast<std::uint16_t>(source->source_blt_color_key.dwColorSpaceHighValue);
    }
    const re2dj::graphics::Rgb565SurfaceView destination_view = {
        destination->pixels, destination->width, destination->height, destination->pitch};
    const re2dj::graphics::LegacyTextureView source_view = {
        source->pixels,
        source->width,
        source->height,
        source->pitch,
        source->texture_identity,
        source->texture_revision,
        key};
    if (!re2dj::graphics::CopyRgb565Rectangle(destination_view,
                                              destination_rectangle.x,
                                              destination_rectangle.y,
                                              source_view,
                                              source_rectangle,
                                              key))
    {
        return DDERR_INVALIDRECT;
    }
    MarkSurfaceDirty(destination);

    const bool is_display_surface =
        (destination->capabilities & (DDSCAPS_PRIMARYSURFACE | DDSCAPS_BACKBUFFER)) != 0;
    if (!is_display_surface || destination->root->render_backend == nullptr)
    {
        return DD_OK;
    }

    const float left = static_cast<float>(destination_rectangle.x);
    const float top = static_cast<float>(destination_rectangle.y);
    const float right = static_cast<float>(destination_rectangle.x + destination_rectangle.width);
    const float bottom = static_cast<float>(destination_rectangle.y + destination_rectangle.height);
    const float source_width = static_cast<float>(source->width);
    const float source_height = static_cast<float>(source->height);
    const float u0 = static_cast<float>(source_rectangle.x) / source_width;
    const float v0 = static_cast<float>(source_rectangle.y) / source_height;
    const float u1 = static_cast<float>(source_rectangle.x + source_rectangle.width) / source_width;
    const float v1 = static_cast<float>(source_rectangle.y + source_rectangle.height) / source_height;
    re2dj::graphics::LegacyDrawCommand command;
    command.vertices = {
        {left, top, 0.0f, 1.0f, 0xffffffff, 0, u0, v0},
        {right, top, 0.0f, 1.0f, 0xffffffff, 0, u1, v0},
        {left, bottom, 0.0f, 1.0f, 0xffffffff, 0, u0, v1},
        {right, bottom, 0.0f, 1.0f, 0xffffffff, 0, u1, v1},
    };
    re2dj::graphics::LegacyFixedFunctionState state;
    // Color keying now discards on its own, so the blit path no longer has to
    // borrow the alpha test to express it.
    state.color_key_enabled = use_source_color_key;
    std::string error;
    if (!destination->root->render_backend->Draw(command,
                                                  state,
                                                  destination->root->width,
                                                  destination->root->height,
                                                  &source_view,
                                                  &error))
    {
        OutputDebugStringA(kOpenGlFailureMessage);
        return DDERR_GENERIC;
    }
    return DD_OK;
}

bool BuildFixedFunctionState(const DeviceFacade& device,
                             re2dj::graphics::LegacyFixedFunctionState* state,
                             std::string* error)
{
    if (state == nullptr || error == nullptr)
    {
        return false;
    }
    const auto& stage = device.texture_stage_states[0];
    if (stage[D3DTSS_COLOROP] != D3DTOP_MODULATE ||
        stage[D3DTSS_COLORARG1] != D3DTA_TEXTURE ||
        stage[D3DTSS_COLORARG2] != D3DTA_DIFFUSE)
    {
        *error = "unsupported Direct3D3 texture color operation";
        return false;
    }
    state->color_key_enabled =
        device.render_states[D3DRENDERSTATE_COLORKEYENABLE] != 0;
    state->alpha_test_enabled =
        device.render_states[D3DRENDERSTATE_ALPHATESTENABLE] != 0;
    state->alpha_reference = static_cast<std::uint8_t>(
        device.render_states[D3DRENDERSTATE_ALPHAREF] & 0xff);
    if (state->alpha_test_enabled &&
        device.render_states[D3DRENDERSTATE_ALPHAFUNC] != D3DCMP_NOTEQUAL)
    {
        *error = "unsupported Direct3D3 alpha comparison function";
        return false;
    }
    state->alpha_function = re2dj::graphics::CompareFunction::kNotEqual;
    state->alpha_blend_enabled =
        device.render_states[D3DRENDERSTATE_ALPHABLENDENABLE] != 0;

    state->depth_test_enabled = device.render_states[D3DRENDERSTATE_ZENABLE] != 0;
    state->depth_write_enabled = device.render_states[D3DRENDERSTATE_ZWRITEENABLE] != 0;
    const auto convert_compare = [](DWORD value,
                                    re2dj::graphics::CompareFunction* output) {
        switch (value)
        {
        case D3DCMP_NEVER:
            *output = re2dj::graphics::CompareFunction::kNever;
            return true;
        case D3DCMP_LESS:
            *output = re2dj::graphics::CompareFunction::kLess;
            return true;
        case D3DCMP_EQUAL:
            *output = re2dj::graphics::CompareFunction::kEqual;
            return true;
        case D3DCMP_LESSEQUAL:
            *output = re2dj::graphics::CompareFunction::kLessEqual;
            return true;
        case D3DCMP_GREATER:
            *output = re2dj::graphics::CompareFunction::kGreater;
            return true;
        case D3DCMP_NOTEQUAL:
            *output = re2dj::graphics::CompareFunction::kNotEqual;
            return true;
        case D3DCMP_GREATEREQUAL:
            *output = re2dj::graphics::CompareFunction::kGreaterEqual;
            return true;
        case D3DCMP_ALWAYS:
            *output = re2dj::graphics::CompareFunction::kAlways;
            return true;
        default:
            return false;
        }
    };
    if (state->depth_test_enabled &&
        !convert_compare(device.render_states[D3DRENDERSTATE_ZFUNC],
                          &state->depth_function))
    {
        *error = "unsupported Direct3D3 depth comparison function";
        return false;
    }

    const auto convert_blend = [](DWORD value,
                                  re2dj::graphics::BlendFactor* output) {
        switch (value)
        {
            case D3DBLEND_ZERO:
                *output = re2dj::graphics::BlendFactor::kZero;
                return true;
            case D3DBLEND_ONE:
                *output = re2dj::graphics::BlendFactor::kOne;
                return true;
            case D3DBLEND_SRCCOLOR:
                *output = re2dj::graphics::BlendFactor::kSourceColor;
                return true;
            case D3DBLEND_INVSRCCOLOR:
                *output = re2dj::graphics::BlendFactor::kInverseSourceColor;
                return true;
            case D3DBLEND_SRCALPHA:
                *output = re2dj::graphics::BlendFactor::kSourceAlpha;
                return true;
            case D3DBLEND_INVSRCALPHA:
                *output = re2dj::graphics::BlendFactor::kInverseSourceAlpha;
                return true;
            default:
                return false;
        }
    };
    if (state->alpha_blend_enabled &&
        (!convert_blend(device.render_states[D3DRENDERSTATE_SRCBLEND],
                        &state->source_blend) ||
         !convert_blend(device.render_states[D3DRENDERSTATE_DESTBLEND],
                        &state->destination_blend)))
    {
        *error = "unsupported Direct3D3 alpha blend factor";
        return false;
    }

    const auto convert_filter = [](DWORD value,
                                   re2dj::graphics::TextureFilter* output) {
        if (value == D3DTFN_POINT)
        {
            *output = re2dj::graphics::TextureFilter::kNearest;
            return true;
        }
        if (value == D3DTFN_LINEAR)
        {
            *output = re2dj::graphics::TextureFilter::kLinear;
            return true;
        }
        return false;
    };
    if (!convert_filter(stage[D3DTSS_MINFILTER], &state->minification_filter) ||
        !convert_filter(stage[D3DTSS_MAGFILTER], &state->magnification_filter))
    {
        *error = "unsupported Direct3D3 texture filter";
        return false;
    }
    error->clear();
    return true;
}

bool IsFullScreenBlackFadeCandidate(
    const re2dj::graphics::LegacyDrawCommand& command,
    const re2dj::graphics::LegacyTextureView* texture,
    std::uint32_t logical_width,
    std::uint32_t logical_height)
{
    if (texture != nullptr || command.topology != re2dj::graphics::PrimitiveTopology::kTriangleStrip ||
        command.vertices.size() != 4 || logical_width == 0 || logical_height == 0)
    {
        return false;
    }
    constexpr float kPositionTolerance = 0.01f;
    float minimum_x = command.vertices.front().x;
    float minimum_y = command.vertices.front().y;
    float maximum_x = minimum_x;
    float maximum_y = minimum_y;
    const std::uint32_t first_color = command.vertices.front().diffuse_argb;
    const std::uint32_t first_rgb = first_color & 0x00ffffffU;
    const std::uint32_t first_alpha = first_color >> 24;
    if (first_rgb != 0 || first_alpha == 0 || first_alpha == 0xff)
    {
        return false;
    }
    for (const auto& vertex : command.vertices)
    {
        minimum_x = (std::min)(minimum_x, vertex.x);
        minimum_y = (std::min)(minimum_y, vertex.y);
        maximum_x = (std::max)(maximum_x, vertex.x);
        maximum_y = (std::max)(maximum_y, vertex.y);
        if ((vertex.diffuse_argb & 0x00ffffffU) != 0 ||
            (vertex.diffuse_argb >> 24) != first_alpha)
        {
            return false;
        }
    }
    return std::fabs(minimum_x) <= kPositionTolerance &&
           std::fabs(minimum_y) <= kPositionTolerance &&
           std::fabs(maximum_x - static_cast<float>(logical_width)) <= kPositionTolerance &&
           std::fabs(maximum_y - static_cast<float>(logical_height)) <= kPositionTolerance;
}

struct ViewportFacade
{
    IDirect3DViewport3 interface_value = {ViewportVtable()};
    volatile LONG references = 1;
    DWORD magic = kViewportMagic;
    RootFacade* root = nullptr;
    D3DVIEWPORT2 viewport = {};
};

D3DMATRIX IdentityMatrix()
{
    D3DMATRIX matrix = {};
    matrix._11 = 1.0f;
    matrix._22 = 1.0f;
    matrix._33 = 1.0f;
    matrix._44 = 1.0f;
    return matrix;
}

void CopyMatrix(const D3DMATRIX& source, re2dj::graphics::LegacyMatrix4x4* destination)
{
    static_assert(sizeof(D3DMATRIX) == sizeof(destination->values));
    std::memcpy(destination->values.data(), &source, sizeof(source));
}

bool BuildLegacyTransformState(const DeviceFacade& device,
                               re2dj::graphics::LegacyTransformState* transform,
                               std::string* error)
{
    if (transform == nullptr || error == nullptr || device.current_viewport == nullptr)
    {
        if (error != nullptr)
        {
            *error = "untransformed draw has no current viewport";
        }
        return false;
    }
    const ViewportFacade* const viewport = ViewportFromInterface(device.current_viewport);
    if (viewport->magic != kViewportMagic)
    {
        *error = "untransformed draw has an invalid viewport";
        return false;
    }
    CopyMatrix(device.transforms[D3DTRANSFORMSTATE_WORLD], &transform->world);
    CopyMatrix(device.transforms[D3DTRANSFORMSTATE_VIEW], &transform->view);
    CopyMatrix(device.transforms[D3DTRANSFORMSTATE_PROJECTION], &transform->projection);
    const D3DVIEWPORT2& source = viewport->viewport;
    transform->viewport.screen_x = static_cast<float>(source.dwX);
    transform->viewport.screen_y = static_cast<float>(source.dwY);
    transform->viewport.screen_width = static_cast<float>(source.dwWidth);
    transform->viewport.screen_height = static_cast<float>(source.dwHeight);
    transform->viewport.clip_x = source.dvClipX;
    transform->viewport.clip_y = source.dvClipY;
    transform->viewport.clip_width = source.dvClipWidth;
    transform->viewport.clip_height = source.dvClipHeight;
    transform->viewport.min_z = source.dvMinZ;
    transform->viewport.max_z = source.dvMaxZ;
    error->clear();
    return true;
}

void ReportTransformDiagnostic(const DeviceFacade& device,
                               DWORD vertex_type,
                               const re2dj::graphics::LegacyTransformState& transform,
                               std::span<const std::byte> source,
                               std::size_t vertex_count,
                               const re2dj::graphics::LegacyDrawCommand& command)
{
    if (device.root == nullptr || ++device.root->transform_diagnostic_count > 128)
    {
        return;
    }
    const auto& viewport = transform.viewport;
    const auto& world = transform.world.values;
    const auto& projection = transform.projection.values;
    float raw_minimum_x = 0.0f;
    float raw_minimum_y = 0.0f;
    float raw_maximum_x = 0.0f;
    float raw_maximum_y = 0.0f;
    if (vertex_count != 0 && source.size() >= 12)
    {
        auto read_float = [](const std::byte* address) {
            float value = 0.0f;
            std::memcpy(&value, address, sizeof(value));
            return value;
        };
        raw_minimum_x = raw_maximum_x = read_float(source.data());
        raw_minimum_y = raw_maximum_y = read_float(source.data() + 4);
        const std::size_t stride = re2dj::graphics::VertexStrideFromFvf(vertex_type);
        for (std::size_t index = 1; index < vertex_count; ++index)
        {
            const std::byte* const vertex = source.data() + index * stride;
            const float x = read_float(vertex);
            const float y = read_float(vertex + 4);
            raw_minimum_x = (std::min)(raw_minimum_x, x);
            raw_minimum_y = (std::min)(raw_minimum_y, y);
            raw_maximum_x = (std::max)(raw_maximum_x, x);
            raw_maximum_y = (std::max)(raw_maximum_y, y);
        }
    }
    float screen_minimum_x = command.vertices.front().x;
    float screen_minimum_y = command.vertices.front().y;
    float screen_maximum_x = screen_minimum_x;
    float screen_maximum_y = screen_minimum_y;
    for (const auto& vertex : command.vertices)
    {
        screen_minimum_x = (std::min)(screen_minimum_x, vertex.x);
        screen_minimum_y = (std::min)(screen_minimum_y, vertex.y);
        screen_maximum_x = (std::max)(screen_maximum_x, vertex.x);
        screen_maximum_y = (std::max)(screen_maximum_y, vertex.y);
    }
    char detail[760] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "TransformDraw:frame=%llu:fvf=0x%08lx:raw=%.3f,%.3f,%.3f,%.3f:screen=%.3f,%.3f,%.3f,%.3f:v0=%.3f,%.3f,%.3f,%.3f:viewport=%lu,%lu,%lu,%lu:clip=%.3f,%.3f,%.3f,%.3f:world=%.3f,%.3f,%.3f,%.3f:projection=%.3f,%.3f,%.3f,%.3f",
                  static_cast<unsigned long long>(device.root->frame_number),
                  static_cast<unsigned long>(vertex_type),
                  raw_minimum_x,
                  raw_minimum_y,
                  raw_maximum_x,
                  raw_maximum_y,
                  screen_minimum_x,
                  screen_minimum_y,
                  screen_maximum_x,
                  screen_maximum_y,
                  command.vertices.front().x,
                  command.vertices.front().y,
                  command.vertices.front().z,
                  command.vertices.front().reciprocal_w,
                  static_cast<unsigned long>(viewport.screen_x),
                  static_cast<unsigned long>(viewport.screen_y),
                  static_cast<unsigned long>(viewport.screen_width),
                  static_cast<unsigned long>(viewport.screen_height),
                  viewport.clip_x,
                  viewport.clip_y,
                  viewport.clip_width,
                  viewport.clip_height,
                  world[0],
                  world[5],
                  world[10],
                  world[12],
                  projection[0],
                  projection[5],
                  projection[10],
                  projection[12]);
    ReportCompositionDiagnostic(device.root, detail);
}

struct VertexBufferFacade
{
    IDirect3DVertexBuffer interface_value = {VertexBufferVtable()};
    volatile LONG references = 1;
    DWORD magic = kVertexBufferMagic;
    RootFacade* root = nullptr;
    re2dj::graphics::LegacyVertexBufferDesc descriptor;
    std::unique_ptr<re2dj::graphics::LegacyVertexBuffer> buffer;
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

VertexBufferFacade* VertexBufferFromInterface(IDirect3DVertexBuffer* self)
{
    return reinterpret_cast<VertexBufferFacade*>(reinterpret_cast<unsigned char*>(self) -
                                                 offsetof(VertexBufferFacade, interface_value));
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
    char qi_buf[96] = {};
    std::snprintf(qi_buf, sizeof(qi_buf),
                  "re2dj:hle:RootQueryInterface iid={%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                  iid.Data1, iid.Data2, iid.Data3,
                  iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
                  iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);
    OutputDebugStringA(qi_buf);
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
    constexpr GUID kIidDirectDraw2 = {
        0xb10f182e, 0x04a7, 0x11d1, {0xa4, 0x5f, 0x00, 0xaa, 0x00, 0xc7, 0x49, 0x68}};
    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IDirectDraw) ||
        IsEqualGUID(iid, kIidDirectDraw2) || IsEqualGUID(iid, IID_IDirectDraw4))
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

HRESULT WINAPI RootGetCaps(IDirectDraw4* self, DDCAPS* driver_caps, DDCAPS* hel_caps)
{
    (void)self;
    OutputDebugStringA("re2dj:hle:IDirectDraw4::GetCaps");
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
    OutputDebugStringA("re2dj:hle:IDirectDraw4::CreateSurface");
    if (descriptor == nullptr || surface == nullptr || outer != nullptr ||
        descriptor->dwSize != sizeof(DDSURFACEDESC2))
    {
        return DDERR_INVALIDPARAMS;
    }
    *surface = nullptr;
    RootFacade* const root = RootFromDirectDraw(self);
    const auto finish = [&](HRESULT result, const SurfaceFacade* created = nullptr) {
        ReportCreateSurfaceDiagnostic(root, *descriptor, created, result);
        return result;
    };
    if ((descriptor->ddsCaps.dwCaps & DDSCAPS_TEXTURE) != 0)
    {
        constexpr DWORD kRequiredFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
                                         DDSD_PIXELFORMAT;
        if ((descriptor->dwFlags & kRequiredFlags) != kRequiredFlags ||
            descriptor->dwWidth == 0 || descriptor->dwHeight == 0 ||
            !IsRgb565Format(descriptor->ddpfPixelFormat))
        {
            return finish(DDERR_INVALIDPIXELFORMAT);
        }
        auto* const texture = new (std::nothrow) SurfaceFacade;
        if (texture == nullptr)
        {
            return finish(DDERR_OUTOFMEMORY);
        }
        texture->root = root;
        texture->width = descriptor->dwWidth;
        texture->height = descriptor->dwHeight;
        texture->bits_per_pixel = 16;
        texture->capabilities = descriptor->ddsCaps.dwCaps;
        texture->diagnostic_id = AllocateSurfaceDiagnosticId(root);
        texture->texture_identity = AllocateSurfaceIdentity(root);
        if (!CreateRgb565GdiBacking(texture))
        {
            delete texture;
            return finish(DDERR_OUTOFMEMORY);
        }
        AddRootReference(root);
        *surface = &texture->interface_value;
        OutputDebugStringA(kCreateTextureSurfaceMessage);
        return finish(DD_OK, texture);
    }
    if ((descriptor->ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN) != 0)
    {
        constexpr DWORD kRequiredFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        if ((descriptor->dwFlags & kRequiredFlags) != kRequiredFlags ||
            descriptor->dwWidth == 0 || descriptor->dwHeight == 0 ||
            ((descriptor->dwFlags & DDSD_PIXELFORMAT) != 0 &&
             !IsRgb565Format(descriptor->ddpfPixelFormat)))
        {
            return finish(DDERR_INVALIDPIXELFORMAT);
        }
        auto* const offscreen = new (std::nothrow) SurfaceFacade;
        if (offscreen == nullptr)
        {
            return finish(DDERR_OUTOFMEMORY);
        }
        offscreen->root = root;
        offscreen->width = descriptor->dwWidth;
        offscreen->height = descriptor->dwHeight;
        offscreen->bits_per_pixel = 16;
        offscreen->capabilities = descriptor->ddsCaps.dwCaps;
        offscreen->diagnostic_id = AllocateSurfaceDiagnosticId(root);
        offscreen->texture_identity = AllocateSurfaceIdentity(root);
        if (!CreateRgb565GdiBacking(offscreen))
        {
            delete offscreen;
            return finish(DDERR_OUTOFMEMORY);
        }
        AddRootReference(root);
        *surface = &offscreen->interface_value;
        return finish(DD_OK, offscreen);
    }
    if ((descriptor->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == 0 ||
        descriptor->dwBackBufferCount != 1)
    {
        return finish(DDERR_UNSUPPORTED);
    }
    auto* const primary = new (std::nothrow) SurfaceFacade;
    auto* const back = new (std::nothrow) SurfaceFacade;
    if (primary == nullptr || back == nullptr)
    {
        delete primary;
        delete back;
        return finish(DDERR_OUTOFMEMORY);
    }
    primary->root = root;
    primary->width = root->width;
    primary->height = root->height;
    primary->bits_per_pixel = root->bits_per_pixel;
    primary->capabilities = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
    primary->diagnostic_id = AllocateSurfaceDiagnosticId(root);
    primary->texture_identity = AllocateSurfaceIdentity(root);
    primary->attached_back_buffer = back;
    back->root = root;
    back->width = root->width;
    back->height = root->height;
    back->bits_per_pixel = root->bits_per_pixel;
    back->capabilities = DDSCAPS_BACKBUFFER | DDSCAPS_3DDEVICE;
    back->diagnostic_id = AllocateSurfaceDiagnosticId(root);
    back->texture_identity = AllocateSurfaceIdentity(root);
    if (!CreateRgb565GdiBacking(primary) || !CreateRgb565GdiBacking(back))
    {
        DestroyGdiBacking(primary);
        DestroyGdiBacking(back);
        delete primary;
        delete back;
        return finish(DDERR_OUTOFMEMORY);
    }
    AddRootReference(root);
    AddRootReference(root);
    *surface = &primary->interface_value;
    return finish(DD_OK, primary);
}

HRESULT WINAPI RootSetCooperativeLevel(IDirectDraw4* self, HWND window, DWORD flags)
{
    char coop_buf[80] = {};
    std::snprintf(coop_buf, sizeof(coop_buf), "re2dj:hle:IDirectDraw4::SetCooperativeLevel hwnd=0x%08x flags=0x%08x",
                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(window)), static_cast<unsigned>(flags));
    OutputDebugStringA(coop_buf);
    RootFacade* const root = RootFromDirectDraw(self);
    if (window == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    if (!ApplyRe2djWindowMode(window, root->width, root->height))
    {
        return DDERR_GENERIC;
    }
    root->window = window;
    root->fps_frequency = {};
    root->fps_interval_start = {};
    root->fps_interval_frames = 0;
    return DD_OK;
}

HRESULT WINAPI RootSetDisplayMode(IDirectDraw4* self,
                                  DWORD width,
                                  DWORD height,
                                  DWORD bits_per_pixel,
                                  DWORD,
                                  DWORD)
{
    char mode_buf[80] = {};
    std::snprintf(mode_buf, sizeof(mode_buf), "re2dj:hle:IDirectDraw4::SetDisplayMode %ux%ux%u",
                  static_cast<unsigned>(width), static_cast<unsigned>(height), static_cast<unsigned>(bits_per_pixel));
    OutputDebugStringA(mode_buf);
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
    facade->render_states[D3DRENDERSTATE_SRCBLEND] = D3DBLEND_ONE;
    facade->render_states[D3DRENDERSTATE_DESTBLEND] = D3DBLEND_ZERO;
    facade->texture_stage_states[0][D3DTSS_COLOROP] = D3DTOP_MODULATE;
    facade->texture_stage_states[0][D3DTSS_COLORARG1] = D3DTA_TEXTURE;
    facade->texture_stage_states[0][D3DTSS_COLORARG2] = D3DTA_DIFFUSE;
    facade->texture_stage_states[0][D3DTSS_MINFILTER] = D3DTFN_POINT;
    facade->texture_stage_states[0][D3DTSS_MAGFILTER] = D3DTFG_POINT;
    facade->transforms[D3DTRANSFORMSTATE_WORLD] = IdentityMatrix();
    facade->transforms[D3DTRANSFORMSTATE_VIEW] = IdentityMatrix();
    facade->transforms[D3DTRANSFORMSTATE_PROJECTION] = IdentityMatrix();
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

HRESULT WINAPI D3dCreateVertexBuffer(IDirect3D3* self,
                                     D3DVERTEXBUFFERDESC* descriptor,
                                     IDirect3DVertexBuffer** vertex_buffer,
                                     DWORD flags,
                                     IUnknown* outer)
{
    if (vertex_buffer == nullptr || descriptor == nullptr || outer != nullptr ||
        descriptor->dwSize < sizeof(D3DVERTEXBUFFERDESC))
    {
        return DDERR_INVALIDPARAMS;
    }
    *vertex_buffer = nullptr;
    char message[160] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:hle:IDirect3D3::CreateVertexBuffer:caps=0x%08lx:fvf=0x%08lx:vertices=%lu:flags=0x%08lx",
                  descriptor->dwCaps,
                  descriptor->dwFVF,
                  static_cast<unsigned long>(descriptor->dwNumVertices),
                  flags);
    OutputDebugStringA(message);
    auto* const facade = new (std::nothrow) VertexBufferFacade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->root = RootFromDirect3d(self);
    facade->descriptor.size = sizeof(D3DVERTEXBUFFERDESC);
    facade->descriptor.caps = descriptor->dwCaps;
    facade->descriptor.fvf = descriptor->dwFVF;
    facade->descriptor.vertex_count = descriptor->dwNumVertices;
    facade->buffer = re2dj::graphics::LegacyVertexBuffer::Create(facade->descriptor);
    if (facade->buffer == nullptr)
    {
        delete facade;
        return DDERR_INVALIDPARAMS;
    }
    AddRootReference(facade->root);
    *vertex_buffer = &facade->interface_value;
    char result_message[160] = {};
    std::snprintf(result_message,
                  sizeof(result_message),
                  "re2dj:hle:IDirect3D3::CreateVertexBuffer:result=%p:vtable=%p",
                  static_cast<void*>(*vertex_buffer),
                  static_cast<void*>((*vertex_buffer)->lpVtbl));
    OutputDebugStringA(result_message);
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
        if (root->render_backend != nullptr && surface->texture_identity != 0)
        {
            root->render_backend->DiscardTexture(surface->texture_identity);
        }
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
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    const SurfaceFacade* traced_source =
        source == nullptr ? nullptr : SurfaceFromInterface(source);
    const auto finish = [&](HRESULT result) {
        ReportBltDiagnostic(
            "Blt", surface, destination, traced_source, source_rectangle, flags, result);
        return result;
    };
    if (source != nullptr)
    {
        constexpr DWORD kSupportedFlags = DDBLT_KEYSRC | DDBLT_WAIT;
        if ((flags & ~kSupportedFlags) != 0 || effects != nullptr)
        {
            return finish(DDERR_UNSUPPORTED);
        }
        SurfaceFacade* const source_surface = SurfaceFromInterface(source);
        re2dj::graphics::Rgb565Rectangle source_region;
        re2dj::graphics::Rgb565Rectangle destination_region;
        if (!BuildSurfaceRectangle(*source_surface, source_rectangle, &source_region) ||
            !BuildSurfaceRectangle(*surface, destination, &destination_region))
        {
            return finish(DDERR_INVALIDRECT);
        }
        return finish(CopySurfaceRectangle(surface,
                                           destination_region,
                                           source_surface,
                                           source_region,
                                           (flags & DDBLT_KEYSRC) != 0));
    }
    if (source_rectangle != nullptr || flags != DDBLT_COLORFILL)
    {
        return finish(DDERR_UNSUPPORTED);
    }
    if (effects == nullptr || effects->dwSize != sizeof(DDBLTFX))
    {
        return finish(DDERR_INVALIDPARAMS);
    }
    if (surface->pixels == nullptr || surface->dc_acquired)
    {
        return finish(DDERR_SURFACEBUSY);
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
        return finish(DDERR_INVALIDRECT);
    }
    const std::uint16_t color = static_cast<std::uint16_t>(effects->dwFillColor);
    auto* const pixels = static_cast<unsigned char*>(surface->pixels);
    for (LONG y = rectangle.top; y < rectangle.bottom; ++y)
    {
        auto* const row = reinterpret_cast<std::uint16_t*>(pixels + y * surface->pitch);
        std::fill(row + rectangle.left, row + rectangle.right, color);
    }
    MarkSurfaceDirty(surface);
    return finish(DD_OK);
}

HRESULT WINAPI SurfaceBltFast(IDirectDrawSurface4* self,
                              DWORD destination_x,
                              DWORD destination_y,
                              IDirectDrawSurface4* source,
                              RECT* source_rectangle,
                              DWORD flags)
{
    constexpr DWORD kSupportedFlags = DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT;
    SurfaceFacade* const destination_surface = SurfaceFromInterface(self);
    SurfaceFacade* const source_surface =
        source == nullptr ? nullptr : SurfaceFromInterface(source);
    RECT destination_rectangle = {static_cast<LONG>(destination_x),
                                  static_cast<LONG>(destination_y),
                                  static_cast<LONG>(destination_x),
                                  static_cast<LONG>(destination_y)};
    const auto finish = [&](HRESULT result) {
        ReportBltDiagnostic("BltFast",
                            destination_surface,
                            &destination_rectangle,
                            source_surface,
                            source_rectangle,
                            flags,
                            result);
        return result;
    };
    if (source == nullptr || (flags & ~kSupportedFlags) != 0)
    {
        return finish(DDERR_UNSUPPORTED);
    }
    re2dj::graphics::Rgb565Rectangle source_region;
    if (!BuildSurfaceRectangle(*source_surface, source_rectangle, &source_region))
    {
        return finish(DDERR_INVALIDRECT);
    }
    const re2dj::graphics::Rgb565Rectangle destination_region = {
        destination_x, destination_y, source_region.width, source_region.height};
    destination_rectangle.right =
        static_cast<LONG>(destination_x + source_region.width);
    destination_rectangle.bottom =
        static_cast<LONG>(destination_y + source_region.height);
    return finish(CopySurfaceRectangle(destination_surface,
                                       destination_region,
                                       source_surface,
                                       source_region,
                                       (flags & DDBLTFAST_SRCCOLORKEY) != 0));
}

HRESULT WINAPI SurfaceFlip(IDirectDrawSurface4* self,
                           IDirectDrawSurface4* override_surface,
                           DWORD)
{
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    ++surface->root->frame_number;
    const auto finish = [&](HRESULT result) {
        ReportSurfaceDiagnostic("Flip", surface, result);
        return result;
    };
    if (surface->attached_back_buffer == nullptr ||
        (override_surface != nullptr &&
         override_surface != &surface->attached_back_buffer->interface_value))
    {
        return finish(DDERR_NOTFLIPPABLE);
    }
    if (surface->root->render_backend != nullptr)
    {
        std::string error;
        const bool presented = surface->root->render_backend->Present(&error);
        Re2djExitIfWindowClosed(surface->root->window);
        if (!presented)
        {
            OutputDebugStringA(kOpenGlFailureMessage);
            return finish(DDERR_GENERIC);
        }
    }
    else
    {
        Re2djExitIfWindowClosed(surface->root->window);
    }
    RecordPresentedFrame(surface->root);
    return finish(DD_OK);
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
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    const auto finish = [&](HRESULT result) {
        ReportSurfaceDiagnostic("GetDC", surface, result);
        return result;
    };
    if (dc == nullptr)
    {
        return finish(DDERR_INVALIDPARAMS);
    }
    *dc = nullptr;
    if (surface->bitmap_dc == nullptr)
    {
        return finish(DDERR_UNSUPPORTED);
    }
    if (surface->dc_acquired)
    {
        return finish(DDERR_DCALREADYCREATED);
    }
    surface->dc_acquired = true;
    *dc = surface->bitmap_dc;
    return finish(DD_OK);
}

HRESULT WINAPI SurfaceIsLost(IDirectDrawSurface4*)
{
    return DD_OK;
}

HRESULT WINAPI SurfaceReleaseDC(IDirectDrawSurface4* self, HDC dc)
{
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    const auto finish = [&](HRESULT result) {
        ReportSurfaceDiagnostic("ReleaseDC", surface, result);
        return result;
    };
    if (!surface->dc_acquired || dc == nullptr || dc != surface->bitmap_dc)
    {
        return finish(DDERR_INVALIDPARAMS);
    }
    surface->dc_acquired = false;
    MarkSurfaceDirty(surface);
    return finish(DD_OK);
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

HRESULT WINAPI TextureLoad(IDirect3DTexture2* self, IDirect3DTexture2* source)
{
    SurfaceFacade* const destination = SurfaceFromTexture(self);
    SurfaceFacade* source_surface = nullptr;
    const auto finish = [&](HRESULT result) {
        ReportTextureLoadDiagnostic(destination, source_surface, result);
        return result;
    };
    if (source == nullptr)
    {
        return finish(DDERR_INVALIDPARAMS);
    }
    if (source == self)
    {
        return finish(DD_OK);
    }
    if (IsBadReadPtr(source, sizeof(*source)) != FALSE)
    {
        return finish(DDERR_INVALIDOBJECT);
    }
    source_surface = SurfaceFromTexture(source);
    if (IsBadReadPtr(source_surface, sizeof(*source_surface)) != FALSE ||
        destination->magic != kSurfaceMagic || source_surface->magic != kSurfaceMagic ||
        destination->root != source_surface->root ||
        (destination->capabilities & DDSCAPS_TEXTURE) == 0 ||
        (source_surface->capabilities & DDSCAPS_TEXTURE) == 0 ||
        destination->pixels == nullptr || source_surface->pixels == nullptr)
    {
        return finish(DDERR_INVALIDOBJECT);
    }
    if (destination->width != source_surface->width ||
        destination->height != source_surface->height ||
        destination->bits_per_pixel != source_surface->bits_per_pixel)
    {
        return finish(D3DERR_TEXTURE_LOAD_FAILED);
    }
    if (destination->dc_acquired || source_surface->dc_acquired)
    {
        return finish(DDERR_SURFACEBUSY);
    }

    const std::size_t row_bytes = static_cast<std::size_t>(destination->width) *
                                  sizeof(std::uint16_t);
    auto* const destination_bytes = static_cast<unsigned char*>(destination->pixels);
    const auto* const source_bytes = static_cast<const unsigned char*>(source_surface->pixels);
    for (DWORD y = 0; y < destination->height; ++y)
    {
        unsigned char* const destination_row = destination_bytes + y * destination->pitch;
        const unsigned char* const source_row = source_bytes + y * source_surface->pitch;
        std::memcpy(destination_row, source_row, row_bytes);
        std::fill(destination_row + row_bytes,
                  destination_row + destination->pitch,
                  static_cast<unsigned char>(0));
    }
    destination->has_source_blt_color_key = source_surface->has_source_blt_color_key;
    destination->source_blt_color_key = source_surface->source_blt_color_key;
    MarkSurfaceDirty(destination);
    return finish(DD_OK);
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
    const unsigned state_index = static_cast<unsigned>(state);
    if (state_index >= 256)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const device = DeviceFromInterface(self);
    const DWORD previous = device->render_states[state_index];
    device->render_states[state_index] = value;
    std::uint8_t& reports = device->render_state_reports[state_index];
    if (reports < 8 && (reports == 0 || previous != value))
    {
        char message[128] = {};
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:hle:render-state:state=%u:value=0x%08x",
                      state_index,
                      static_cast<unsigned>(value));
        OutputDebugStringA(message);
        char detail[160] = {};
        std::snprintf(detail,
                      sizeof(detail),
                      "RenderState:frame=%llu:state=%u:value=0x%08x",
                      static_cast<unsigned long long>(device->root->frame_number),
                      state_index,
                      static_cast<unsigned>(value));
        ReportCompositionDiagnostic(device->root, detail);
        ++reports;
    }
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
    DeviceFacade* const device = DeviceFromInterface(self);
    const DWORD previous = device->texture_stage_states[stage][state_index];
    device->texture_stage_states[stage][state_index] = value;
    std::uint8_t& reports = device->texture_stage_state_reports[stage][state_index];
    if (reports < 8 && (reports == 0 || previous != value))
    {
        char message[144] = {};
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:hle:texture-stage-state:stage=%u:state=%u:value=0x%08x",
                      static_cast<unsigned>(stage),
                      state_index,
                      static_cast<unsigned>(value));
        OutputDebugStringA(message);
        ++reports;
    }
    return DD_OK;
}

HRESULT WINAPI DeviceDrawPrimitive(IDirect3DDevice3* self,
                                   D3DPRIMITIVETYPE primitive,
                                   DWORD vertex_type,
                                   void* vertices,
                                   DWORD vertex_count,
                                   DWORD flags)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    const bool is_triangle_strip = primitive == D3DPT_TRIANGLESTRIP && vertex_count >= 3;
    const bool is_triangle_list =
        primitive == D3DPT_TRIANGLELIST && vertex_count >= 3 && vertex_count % 3 == 0;
    const bool is_line_list =
        primitive == D3DPT_LINELIST && vertex_count >= 2 && vertex_count % 2 == 0;
    const bool is_transformed = vertex_type == D3DFVF_TLVERTEX;
    const bool is_untransformed =
        vertex_type == D3DFVF_VERTEX || vertex_type == D3DFVF_LVERTEX;
    const std::size_t vertex_stride =
        is_transformed ? re2dj::graphics::kTransformedLitVertexStride
                       : re2dj::graphics::VertexStrideFromFvf(vertex_type);
    if ((!is_triangle_strip && !is_triangle_list && !is_line_list) ||
        (!is_transformed && !is_untransformed) || vertices == nullptr ||
        !re2dj::graphics::AreLegacyDrawFlagsSupported(flags) ||
        vertex_stride == 0 ||
        vertex_count > (std::numeric_limits<std::size_t>::max)() / vertex_stride)
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             vertex_type,
                             vertex_count,
                             flags,
                             DDERR_UNSUPPORTED,
                             "unsupported-arguments");
        return DDERR_UNSUPPORTED;
    }
    const std::size_t bytes = static_cast<std::size_t>(vertex_count) * vertex_stride;
    if (IsBadReadPtr(vertices, bytes) != FALSE)
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             vertex_type,
                             vertex_count,
                             flags,
                             DDERR_INVALIDPARAMS,
                             "invalid-vertices");
        return DDERR_INVALIDPARAMS;
    }
    re2dj::graphics::LegacyDrawCommand command;
    std::string error;
    const re2dj::graphics::PrimitiveTopology topology =
        is_line_list ? re2dj::graphics::PrimitiveTopology::kLineList
                     : is_triangle_list ? re2dj::graphics::PrimitiveTopology::kTriangleList
                                        : re2dj::graphics::PrimitiveTopology::kTriangleStrip;
    const std::span<const std::byte> vertex_bytes(
        static_cast<const std::byte*>(vertices), bytes);
    bool decoded = false;
    if (is_transformed)
    {
        decoded = re2dj::graphics::DecodeTransformedLitVertices(
            vertex_bytes, vertex_count, topology, &command, &error);
    }
    else
    {
        re2dj::graphics::LegacyTransformState transform;
        const bool transform_built = BuildLegacyTransformState(*device, &transform, &error);
        if (transform_built)
        {
            decoded = re2dj::graphics::DecodeUntransformedVertices(vertex_bytes,
                                                                    vertex_count,
                                                                    vertex_type,
                                                                    topology,
                                                                    transform,
                                                                    &command,
                                                                    &error);
            if (decoded)
            {
                ReportTransformDiagnostic(*device,
                                          vertex_type,
                                          transform,
                                          vertex_bytes,
                                          vertex_count,
                                          command);
            }
        }
    }
    if (!decoded)
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             vertex_type,
                             vertex_count,
                             flags,
                             DDERR_INVALIDPARAMS,
                             error.c_str());
        return DDERR_INVALIDPARAMS;
    }

    RootFacade* const root = device->root;
    if (root->window == nullptr)
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             vertex_type,
                             vertex_count,
                             flags,
                             DDERR_NOCOOPERATIVELEVELSET,
                             "no-window");
        return DDERR_NOCOOPERATIVELEVELSET;
    }
    if (root->render_backend == nullptr)
    {
        auto* const backend = new (std::nothrow) re2dj::graphics::Sdl3OpenGlBackend;
        const re2dj::graphics::Sdl3OpenGlWindowConfig window_config = {
            root->window, root->width, root->height, "re2DJ"};
        if (backend == nullptr || !backend->Initialize(window_config, &error) ||
            !ApplyRe2djWindowMode(root->window, root->width, root->height))
        {
            delete backend;
            OutputDebugStringA(kOpenGlFailureMessage);
            ReportDrawDiagnostic(device,
                                 primitive,
                                 vertex_type,
                                 vertex_count,
                                 flags,
                                 DDERR_GENERIC,
                                 "backend-initialize");
            return DDERR_GENERIC;
        }
        root->render_backend = backend;
    }

    re2dj::graphics::LegacyTextureView texture_view;
    const re2dj::graphics::LegacyTextureView* texture = nullptr;
    if (device->texture_stage_zero != nullptr)
    {
        const SurfaceFacade* const surface = SurfaceFromTexture(device->texture_stage_zero);
        texture_view.pixels = surface->pixels;
        texture_view.width = surface->width;
        texture_view.height = surface->height;
        texture_view.pitch = surface->pitch;
        texture_view.identity = surface->texture_identity;
        texture_view.revision = surface->texture_revision;
        texture_view.source_color_key.enabled = surface->has_source_blt_color_key;
        texture_view.source_color_key.low =
            static_cast<std::uint16_t>(surface->source_blt_color_key.dwColorSpaceLowValue);
        texture_view.source_color_key.high =
            static_cast<std::uint16_t>(surface->source_blt_color_key.dwColorSpaceHighValue);
        texture = &texture_view;
    }
    re2dj::graphics::LegacyFixedFunctionState fixed_function_state;
    const bool state_built = BuildFixedFunctionState(*device, &fixed_function_state, &error);
    if (state_built && !fixed_function_state.alpha_blend_enabled &&
        !fixed_function_state.alpha_test_enabled &&
        IsFullScreenBlackFadeCandidate(command,
                                       texture,
                                       root->width,
                                       root->height))
    {
        fixed_function_state.alpha_blend_enabled = true;
        fixed_function_state.source_blend = re2dj::graphics::BlendFactor::kSourceAlpha;
        fixed_function_state.destination_blend =
            re2dj::graphics::BlendFactor::kInverseSourceAlpha;
        fixed_function_state.fade_compatibility_applied = true;
    }
    const bool drawn = state_built && root->render_backend->Draw(command,
                                                                 fixed_function_state,
                                                                 root->width,
                                                                 root->height,
                                                                 texture,
                                                                 &error);
    if (!drawn)
    {
        if (!device->draw_failure_reported)
        {
            OutputDebugStringA(kOpenGlFailureMessage);
            char message[256] = {};
            std::snprintf(message,
                          sizeof(message),
                          "re2dj:hle:draw-failure:%s",
                          error.c_str());
            OutputDebugStringA(message);
            device->draw_failure_reported = true;
        }
        ReportDrawDiagnostic(device,
                             primitive,
                             vertex_type,
                             vertex_count,
                             flags,
                             DDERR_GENERIC,
                             error.c_str());
        return DDERR_GENERIC;
    }
    if (!device->draw_success_reported)
    {
        OutputDebugStringA(kDrawPrimitiveMessage);
        device->draw_success_reported = true;
    }
    ReportDrawDiagnostic(
        device, primitive, vertex_type, vertex_count, flags, DD_OK, "success");
    ReportLateDrawDiagnostic(device, command, fixed_function_state, flags, vertex_type);
    return DD_OK;
}

HRESULT WINAPI DeviceDrawIndexedPrimitiveVB(IDirect3DDevice3* self,
                                            D3DPRIMITIVETYPE primitive,
                                            IDirect3DVertexBuffer* vertex_buffer,
                                            WORD* indices,
                                            DWORD index_count,
                                            DWORD flags)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    if (vertex_buffer == nullptr || indices == nullptr || index_count == 0 ||
        index_count > (std::numeric_limits<std::size_t>::max)() / sizeof(WORD) ||
        IsBadReadPtr(vertex_buffer, sizeof(*vertex_buffer)) != FALSE ||
        IsBadReadPtr(indices, static_cast<std::size_t>(index_count) * sizeof(WORD)) != FALSE ||
        vertex_buffer->lpVtbl != VertexBufferVtable())
    {
        ReportDrawDiagnostic(
            device, primitive, 0, index_count, flags, DDERR_INVALIDPARAMS, "invalid-indexed-vb");
        return DDERR_INVALIDPARAMS;
    }

    VertexBufferFacade* const facade = VertexBufferFromInterface(vertex_buffer);
    if (IsBadReadPtr(facade, sizeof(*facade)) != FALSE || facade->magic != kVertexBufferMagic ||
        facade->root != device->root || facade->buffer == nullptr)
    {
        ReportDrawDiagnostic(
            device, primitive, 0, index_count, flags, DDERR_INVALIDPARAMS, "foreign-indexed-vb");
        return DDERR_INVALIDPARAMS;
    }
    if (facade->buffer->locked())
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             facade->descriptor.fvf,
                             index_count,
                             flags,
                             D3DERR_VERTEXBUFFERLOCKED,
                             "locked-indexed-vb");
        return D3DERR_VERTEXBUFFERLOCKED;
    }

    const std::span<const std::uint16_t> index_values(
        reinterpret_cast<const std::uint16_t*>(indices), index_count);
    std::vector<std::byte> expanded;
    if (!re2dj::graphics::ExpandIndexedVertices(facade->buffer->vertices(),
                                                facade->buffer->stride(),
                                                facade->descriptor.vertex_count,
                                                index_values,
                                                &expanded))
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             facade->descriptor.fvf,
                             index_count,
                             flags,
                             DDERR_INVALIDPARAMS,
                             "invalid-indexed-vertices");
        return DDERR_INVALIDPARAMS;
    }
    return DeviceDrawPrimitive(self,
                               primitive,
                               facade->descriptor.fvf,
                               expanded.data(),
                               index_count,
                               flags);
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

HRESULT WINAPI VbQueryInterface(IDirect3DVertexBuffer* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic)
    {
        return E_FAIL;
    }
    if (!IsEqualGUID(iid, IID_IUnknown) && !IsEqualGUID(iid, IID_IDirect3DVertexBuffer))
    {
        return E_NOINTERFACE;
    }
    *object = self;
    VbAddRef(self);
    return S_OK;
}

ULONG WINAPI VbAddRef(IDirect3DVertexBuffer* self)
{
    return static_cast<ULONG>(InterlockedIncrement(&VertexBufferFromInterface(self)->references));
}

ULONG WINAPI VbRelease(IDirect3DVertexBuffer* self)
{
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    const LONG references = InterlockedDecrement(&facade->references);
    if (references == 0)
    {
        RootFacade* const root = facade->root;
        facade->magic = 0;
        delete facade;
        ReleaseRootReference(root);
    }
    return static_cast<ULONG>(references);
}

HRESULT WINAPI VbLock(IDirect3DVertexBuffer* self, DWORD flags, void** data, DWORD* size)
{
    char entry_message[192] = {};
    std::snprintf(entry_message,
                  sizeof(entry_message),
                  "re2dj:hle:IDirect3DVertexBuffer::Lock:entry:self=%p:vtable=%p:data=%p:size=%p:flags=0x%08lx",
                  static_cast<void*>(self),
                  self == nullptr ? nullptr : static_cast<void*>(self->lpVtbl),
                  static_cast<void*>(data),
                  static_cast<void*>(size),
                  flags);
    OutputDebugStringA(entry_message);
    if (data == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *data = nullptr;
    if (size != nullptr)
    {
        *size = 0;
    }
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic || facade->buffer == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    std::span<std::byte> vertices = facade->buffer->Lock();
    if (vertices.empty())
    {
        return D3DERR_VERTEXBUFFERLOCKED;
    }
    char message[128] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:hle:IDirect3DVertexBuffer::Lock:success:bytes=%lu:output=%p:flags=0x%08lx",
                  static_cast<unsigned long>(vertices.size()),
                  static_cast<void*>(vertices.data()),
                  flags);
    OutputDebugStringA(message);
    *data = vertices.data();
    if (size != nullptr)
    {
        *size = static_cast<DWORD>(vertices.size());
    }
    return DD_OK;
}

HRESULT WINAPI VbUnlock(IDirect3DVertexBuffer* self)
{
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic || facade->buffer == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    if (!facade->buffer->Unlock())
    {
        OutputDebugStringA("re2dj:hle:IDirect3DVertexBuffer::Unlock:not-locked");
        return DDERR_NOTLOCKED;
    }
    OutputDebugStringA("re2dj:hle:IDirect3DVertexBuffer::Unlock");
    return DD_OK;
}

HRESULT WINAPI VbProcessVertices(IDirect3DVertexBuffer* self,
                                 DWORD,
                                 DWORD,
                                 DWORD,
                                 IDirect3DVertexBuffer*,
                                 DWORD,
                                 IDirect3DDevice3*,
                                 DWORD)
{
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    OutputDebugStringA("re2dj:hle:IDirect3DVertexBuffer::ProcessVertices");
    return E_NOTIMPL;
}

HRESULT WINAPI VbGetVertexBufferDesc(IDirect3DVertexBuffer* self, D3DVERTEXBUFFERDESC* descriptor)
{
    if (descriptor == nullptr || descriptor->dwSize < sizeof(D3DVERTEXBUFFERDESC))
    {
        return DDERR_INVALIDPARAMS;
    }
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic || facade->buffer == nullptr)
    {
        return DDERR_INVALIDOBJECT;
    }
    descriptor->dwSize = facade->descriptor.size;
    descriptor->dwCaps = facade->descriptor.caps;
    descriptor->dwFVF = facade->descriptor.fvf;
    descriptor->dwNumVertices = facade->descriptor.vertex_count;
    OutputDebugStringA("re2dj:hle:IDirect3DVertexBuffer::GetVertexBufferDesc");
    return DD_OK;
}

HRESULT WINAPI VbOptimize(IDirect3DVertexBuffer* self, IDirect3DDevice3*, DWORD)
{
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    OutputDebugStringA("re2dj:hle:IDirect3DVertexBuffer::Optimize");
    return E_NOTIMPL;
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
