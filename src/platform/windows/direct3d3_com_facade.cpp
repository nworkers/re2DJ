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
#include "runtime_log.h"
#include "re2dj/graphics/legacy_texture.h"
#include "re2dj/graphics/legacy_transform.h"
#include "re2dj/graphics/legacy_vertex_buffer.h"
#include "re2dj/graphics/present_interval_histogram.h"

#include "guest_wait_accounting.h"
#include "re2dj/graphics/sdl3_opengl_backend.h"
#include "directdraw_legacy_interop.h"
#include "directx_abi_windows.h"
#include "graphics_trace_log.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/direct3d_device.h"
#include "re2dj/directx/direct3d_draw.h"
#include "re2dj/directx/direct3d_vertex_buffer.h"
#include "re2dj/directx/directdraw_description.h"
#include "re2dj/directx/directdraw_display.h"
#include "re2dj/directx/directdraw_surface.h"
#include "host_window_shell.h"
#include "osd_host.h"
#include "timer_resolution_probe.h"
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
// Direct3D 7 defines LIGHTING at render-state index 137. The Direct3D 6
// headers used by this facade do not expose that later enum name.
constexpr unsigned kD3dRenderStateLighting = 137;
// These Direct3D 7 texture-stage/transform constants are absent from the
// Direct3D 6 headers used by this facade. They are logged without applying
// them until the original runtime's values are confirmed.
constexpr unsigned kD3dTextureStageTexcoordIndex = 11;
constexpr unsigned kD3dTextureStageTransformFlags = 24;
constexpr unsigned kD3dTextureTransform0 = 16;
constexpr DWORD kD3dCullNone = 1;
constexpr DWORD kD3dCullClockwise = 2;
constexpr DWORD kD3dCullCounterClockwise = 3;

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
HRESULT WINAPI SurfaceAddAttachedSurface(IDirectDrawSurface4* self,
                                         IDirectDrawSurface4* attachment);
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
HRESULT WINAPI SurfaceLock(IDirectDrawSurface4* self,
                           RECT* rect,
                           DDSURFACEDESC2* descriptor,
                           DWORD flags,
                           HANDLE event);
HRESULT WINAPI SurfaceUnlock(IDirectDrawSurface4* self, RECT* rect);

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
HRESULT WINAPI DeviceDrawPrimitiveVB(IDirect3DDevice3* self,
                                     D3DPRIMITIVETYPE primitive,
                                     IDirect3DVertexBuffer* vertex_buffer,
                                     DWORD start_vertex,
                                     DWORD vertex_count,
                                     DWORD flags);
HRESULT WINAPI DeviceSetRenderTarget(IDirect3DDevice3* self,
                                     IDirectDrawSurface4* surface,
                                     DWORD flags);
HRESULT WINAPI DeviceGetRenderTarget(IDirect3DDevice3* self,
                                     IDirectDrawSurface4** surface);

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
        table.AddAttachedSurface = SurfaceAddAttachedSurface;
        table.Blt = SurfaceBlt;
        table.BltFast = SurfaceBltFast;
        table.Flip = SurfaceFlip;
        table.GetAttachedSurface = SurfaceGetAttachedSurface;
        table.GetCaps = SurfaceGetCaps;
        table.GetDC = SurfaceGetDC;
        table.GetPixelFormat = SurfaceGetPixelFormat;
        table.GetSurfaceDesc = SurfaceGetSurfaceDesc;
        table.IsLost = SurfaceIsLost;
        table.Lock = SurfaceLock;
        table.ReleaseDC = SurfaceReleaseDC;
        table.Restore = SurfaceRestore;
        table.SetColorKey = SurfaceSetColorKey;
        table.Unlock = SurfaceUnlock;
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
        table.DrawPrimitiveVB = DeviceDrawPrimitiveVB;
        table.DrawIndexedPrimitiveVB = DeviceDrawIndexedPrimitiveVB;
        table.SetRenderTarget = DeviceSetRenderTarget;
        table.GetRenderTarget = DeviceGetRenderTarget;
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
    // The tables this root installs on the surfaces, devices, and vertex
    // buffers it creates. A later interface version hands its own tables in
    // here so the objects it receives speak that version while staying the same
    // objects this file implements. Null selects the DirectX 6 table.
    const IDirectDrawSurface4Vtbl* surface_vtable = nullptr;
    const IDirect3DDevice3Vtbl* device_vtable = nullptr;
    const IDirect3DVertexBufferVtbl* vertex_buffer_vtable = nullptr;
    // The window and mode the guest set, under the shared core's rules. The
    // window is kept typed as well, for the Win32 calls made on it.
    re2dj::directx::DirectDrawDisplay display;
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
    std::uint32_t music_select_disc_diagnostic_count = 0;
    std::uint32_t transform_diagnostic_count = 0;
    // Per-frame draw accounting, summarised once at present time. A line per
    // draw would be thousands per second, and the question these answer - does
    // the guest draw at all while the screen changes - only needs the totals.
    std::uint32_t frame_draw_calls = 0;
    std::uint32_t frame_draw_vertices = 0;
    std::uint32_t frame_textured_draw_calls = 0;
    std::uint32_t frame_draw_summary_count = 0;
    // The counter reading at the previous presented frame, so each summary can
    // say how long the frame before it stayed on screen. A guest that stalls
    // without presenting shows up here and nowhere else.
    LARGE_INTEGER frame_summary_previous_counter = {};
    // The first transformed vertex's diffuse colour of the frame. A guest that
    // fades a full-screen quad does it by modulating this, so the sequence of
    // these values is the fade curve it actually asked for.
    std::uint32_t frame_first_diffuse = 0;
    bool frame_first_diffuse_seen = false;
    // A fade is often an overlay quad drawn last, so the final draw's colour is
    // recorded alongside the first one.
    std::uint32_t frame_last_diffuse = 0;
    std::uint64_t frame_number = 0;
    // Set when the guest made a flipping primary. It then presents by handing
    // buffers it owns to the display, so a frame in which it redraws only part
    // of the screen needs the rest still there and the render target must not
    // be cleared between frames. A guest that presents by copying a whole
    // surface onto the primary overwrites the screen every time and starts
    // from nothing.
    bool presentation_retains_frames = false;
    // The surface the guest presents from. A full-surface color fill or D3D
    // target clear aimed at it clears what is about to be shown.
    SurfaceFacade* presentation_surface = nullptr;
    // A guest can clear before the first draw creates the backend. Keep the
    // latest full-target color until the logical render target exists.
    bool pending_render_target_clear = false;
    std::uint16_t pending_render_target_clear_color = 0;
    LARGE_INTEGER fps_frequency = {};
    LARGE_INTEGER fps_interval_start = {};
    std::uint32_t fps_interval_frames = 0;
    // Spacing between presents, so a run records the shape of the guest's
    // pacing and not only its one-second average. Kept here rather than in
    // SurfaceFlip because 3rd presents through Blt and never calls Flip.
    re2dj::graphics::PresentIntervalHistogram present_intervals;
    // Time spent inside the backend's Present. Read against the interval
    // above, this says how much of a guest's frame period the HLE itself
    // occupies, which an interval alone cannot separate from guest work.
    re2dj::graphics::PresentIntervalHistogram present_costs;
    LARGE_INTEGER present_previous_counter = {};
    std::uint32_t present_interval_summaries = 0;
    re2dj::graphics::Sdl3OpenGlBackend* render_backend = nullptr;
    // Whether the backend's software pacing was already recorded.
    bool pacing_reported = false;
};

// Calls the backend's Present and records how long it took. Every present site
// goes through here so the cost series covers Flip and both Blt paths.
bool PresentAndRecordCost(RootFacade* root, std::string* error)
{
    LARGE_INTEGER started = {};
    const bool timed = root->fps_frequency.QuadPart > 0 &&
                       QueryPerformanceCounter(&started) != FALSE;
    const bool presented = root->render_backend->Present(error);
    if (!root->pacing_reported && root->render_backend->software_pacing_engaged())
    {
        root->pacing_reported = true;
        re2dj::platform::windows::WriteGraphicsTraceFormat("re2dj:hle:present-sync:software-pacing=1");
    }
    re2dj::platform::windows::ReportOsdState();
    LARGE_INTEGER finished = {};
    if (timed && QueryPerformanceCounter(&finished) != FALSE)
    {
        root->present_costs.Add(1000.0 *
                                static_cast<double>(finished.QuadPart - started.QuadPart) /
                                static_cast<double>(root->fps_frequency.QuadPart));
    }
    return presented;
}

// One line per closed FPS window. Formatting happens here rather than on the
// present path, which only increments a bucket.
void ReportPresentIntervalSummary(RootFacade* root)
{
    constexpr std::uint32_t kMaximumPresentIntervalSummaries = 120;
    if (root->present_intervals.samples() == 0)
    {
        return;
    }
    if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
        root->present_interval_summaries >= kMaximumPresentIntervalSummaries)
    {
        root->present_intervals.Reset();
        return;
    }
    ++root->present_interval_summaries;
    const auto summary = root->present_intervals.Summarize();
    char buckets[160] = {};
    int written = 0;
    for (const auto& bucket : summary.top_buckets)
    {
        if (bucket.count == 0)
        {
            break;
        }
        const int added = std::snprintf(buckets + written,
                                        sizeof(buckets) - static_cast<std::size_t>(written),
                                        "%s%.2f=%u",
                                        written == 0 ? "" : ",",
                                        bucket.lower_milliseconds,
                                        bucket.count);
        if (added <= 0 || static_cast<std::size_t>(written + added) >= sizeof(buckets))
        {
            break;
        }
        written += added;
    }
    const auto cost = root->present_costs.Summarize();
    re2dj::platform::windows::WriteGraphicsTraceFormat(
        "re2dj:hle:present-interval:frames=%u:mean=%.2f:min=%.2f:p50=%.2f:p95=%.2f:max=%.2f"
        ":top=%s:cost_mean=%.2f:cost_p50=%.2f:cost_p95=%.2f:cost_max=%.2f",
        summary.samples,
        summary.mean_milliseconds,
        summary.minimum_milliseconds,
        summary.median_milliseconds,
        summary.percentile95_milliseconds,
        summary.maximum_milliseconds,
        buckets,
        cost.mean_milliseconds,
        cost.median_milliseconds,
        cost.percentile95_milliseconds,
        cost.maximum_milliseconds);
    root->present_intervals.Reset();
    root->present_costs.Reset();

    // Reported in the same window as the intervals above, so the wait time is
    // attributable to the frames it was measured across. Stays silent when the
    // wrappers were never patched in, rather than printing zeroes that would
    // read as "the guest does not wait".
    re2dj::platform::windows::GuestWaitSummary waits;
    if (re2dj::platform::windows::TakeGuestWaitSummary(&waits))
    {
        re2dj::platform::windows::WriteGraphicsTraceFormat(
            "re2dj:hle:guest-wait:sleep_calls=%u:sleep_ms=%.2f:sleep_requested_ms=%.2f"
            ":sleep_p50=%.2f:sleep_max=%.2f:wait_calls=%u:wait_ms=%.2f:wait_p50=%.2f"
            ":wait_max=%.2f:time_calls=%u",
            waits.sleep_calls,
            waits.sleep_milliseconds,
            waits.requested_sleep_milliseconds,
            waits.sleep_durations.median_milliseconds,
            waits.sleep_durations.maximum_milliseconds,
            waits.wait_calls,
            waits.wait_milliseconds,
            waits.wait_durations.median_milliseconds,
            waits.wait_durations.maximum_milliseconds,
            waits.time_query_calls);
    }

    re2dj::platform::windows::GuestSleepModelSummary model;
    if (re2dj::platform::windows::TakeGuestSleepModelSummary(&model))
    {
        re2dj::platform::windows::WriteGraphicsTraceFormat(
            "re2dj:hle:guest-sleep-model:pairs=%u:req_mean=%.2f:req_sd=%.2f:awake_mean=%.2f"
            ":awake_sd=%.2f:period_mean=%.2f:period_sd=%.2f:corr=%.3f",
            model.pairs,
            model.requested_mean_milliseconds,
            model.requested_deviation_milliseconds,
            model.awake_mean_milliseconds,
            model.awake_deviation_milliseconds,
            model.period_mean_milliseconds,
            model.period_deviation_milliseconds,
            model.correlation);
    }

    re2dj::platform::windows::NoteTimerResolution("frame-window");
}

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
    // The first present of a run has no predecessor, so it starts the series
    // instead of contributing a meaningless interval from zero.
    if (root->present_previous_counter.QuadPart != 0)
    {
        root->present_intervals.Add(
            1000.0 *
            static_cast<double>(now.QuadPart - root->present_previous_counter.QuadPart) /
            static_cast<double>(root->fps_frequency.QuadPart));
    }
    root->present_previous_counter = now;
    const LONGLONG elapsed_ticks = now.QuadPart - root->fps_interval_start.QuadPart;
    if (elapsed_ticks < root->fps_frequency.QuadPart)
    {
        return;
    }
    const double fps = static_cast<double>(root->fps_interval_frames) *
                       static_cast<double>(root->fps_frequency.QuadPart) /
                       static_cast<double>(elapsed_ticks);
    Re2djUpdateWindowTitle(root->window, fps);
    ReportPresentIntervalSummary(root);
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
    // A depth buffer the guest attached with AddAttachedSurface. DirectDraw
    // lets a guest read its attachments back, and one that stores the result
    // without checking would keep a null pointer if this were not recorded.
    SurfaceFacade* attached_depth_buffer = nullptr;
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
    bool draw_failure_diagnostic_reported = false;
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
    // The area the last Lock handed out, which Unlock puts back on the
    // backend's render target when this is the surface the guest presents
    // from.
    bool locked = false;
    DWORD lock_x = 0;
    DWORD lock_y = 0;
    DWORD lock_width = 0;
    DWORD lock_height = 0;
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
    // The shared core's device state. DirectX 7 sets the viewport there
    // instead of through a viewport object; the draw path reads
    // `current_viewport` when the guest attached one and falls back to the
    // state's viewport otherwise.
    re2dj::directx::DeviceState state;
    bool draw_success_reported = false;
    bool draw_failure_reported = false;
    std::array<std::uint8_t, 256> render_state_reports = {};
    std::array<std::array<std::uint8_t, 64>, 8> texture_stage_state_reports = {};
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

void FillSurfaceWithColor(SurfaceFacade* surface, std::uint16_t color)
{
    if (surface == nullptr || surface->pixels == nullptr || surface->pitch == 0)
    {
        return;
    }
    auto* const pixels = static_cast<unsigned char*>(surface->pixels);
    for (DWORD y = 0; y < surface->height; ++y)
    {
        auto* const row = reinterpret_cast<std::uint16_t*>(pixels + y * surface->pitch);
        std::fill(row, row + surface->width, color);
    }
    MarkSurfaceDirty(surface);
}

std::uint16_t Rgb565FromD3dColor(D3DCOLOR color)
{
    return re2dj::directx::Rgb565FromD3dColor(static_cast<std::uint32_t>(color));
}

bool RequestRenderTargetClear(RootFacade* root,
                              std::uint16_t color,
                              std::string* error)
{
    if (root == nullptr || error == nullptr)
    {
        return false;
    }
    if (root->render_backend == nullptr)
    {
        root->pending_render_target_clear = true;
        root->pending_render_target_clear_color = color;
        error->clear();
        return true;
    }
    if (!root->render_backend->ClearRenderTarget(color, error))
    {
        return false;
    }
    root->pending_render_target_clear = false;
    error->clear();
    return true;
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
        (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
         ++root->create_surface_diagnostic_count > kMaximumCreateSurfaceDiagnostics))
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
    if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
        ++(*diagnostic_count) > maximum_diagnostics)
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
    if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
        ++diagnostic_count > maximum_diagnostics)
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
    if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
        ++destination->root->texture_load_diagnostic_count > kMaximumTextureLoadDiagnostics)
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
                          const char* reason,
                          const re2dj::graphics::LegacyDrawCommand* command = nullptr)
{
    // This runs on the draw path, so it is off unless a draw-level
    // investigation asked for it. See graphics_trace_log.h.
    if (!re2dj::platform::windows::AreGraphicsDrawDiagnosticsEnabled())
    {
        return;
    }
    if (device == nullptr || device->root == nullptr)
    {
        return;
    }
    SurfaceFacade* texture_surface = device->texture_stage_zero == nullptr
                                         ? nullptr
                                         : SurfaceFromTexture(device->texture_stage_zero);
    const bool complete_capture =
        re2dj::platform::windows::AreCompleteDiagnosticsEnabled();
    if (result == DD_OK && texture_surface != nullptr &&
        texture_surface->draw_diagnostic_reported && !complete_capture)
    {
        return;
    }
    constexpr std::uint32_t kMaximumDrawFailureDiagnostics = 64;
    constexpr std::uint32_t kMaximumUntexturedDrawDiagnostics = 16;
    const bool first_texture_failure = result != DD_OK && texture_surface != nullptr &&
                                       !texture_surface->draw_failure_diagnostic_reported;
    if (!complete_capture && result != DD_OK && !first_texture_failure &&
        ++device->root->draw_failure_diagnostic_count > kMaximumDrawFailureDiagnostics)
    {
        return;
    }
    if (!complete_capture && result == DD_OK && texture_surface == nullptr &&
        ++device->root->untextured_draw_diagnostic_count > kMaximumUntexturedDrawDiagnostics)
    {
        return;
    }
    if (!complete_capture && result == DD_OK && texture_surface != nullptr)
    {
        texture_surface->draw_diagnostic_reported = true;
    }
    if (!complete_capture && first_texture_failure)
    {
        texture_surface->draw_failure_diagnostic_reported = true;
    }
    char bounds[128] = "unavailable";
    if (command != nullptr && !command->vertices.empty())
    {
        float left = command->vertices.front().x;
        float top = command->vertices.front().y;
        float right = left;
        float bottom = top;
        for (const auto& vertex : command->vertices)
        {
            left = (std::min)(left, vertex.x);
            top = (std::min)(top, vertex.y);
            right = (std::max)(right, vertex.x);
            bottom = (std::max)(bottom, vertex.y);
        }
        std::snprintf(bounds, sizeof(bounds), "%.3f,%.3f,%.3f,%.3f", left, top, right, bottom);
    }
    const auto& stage = device->state.texture_stage_states[0];
    char detail[1024] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "DrawPrimitive:texture=%lu:primitive=%lu:fvf=0x%08lx:vertices=%lu:flags=0x%08lx:result=0x%08lx:reason=%s:colorop=%lu:colorarg1=0x%08lx:colorarg2=0x%08lx:alphatest=%lu:alphafunc=%lu:blend=%lu:srcblend=%lu:dstblend=%lu:minfilter=%lu:magfilter=%lu:frame=%llu:bounds=%s",
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
                      device->state.render_states[D3DRENDERSTATE_ALPHATESTENABLE]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_ALPHAFUNC]),
                  static_cast<unsigned long>(
                      device->state.render_states[D3DRENDERSTATE_ALPHABLENDENABLE]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_SRCBLEND]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_DESTBLEND]),
                  static_cast<unsigned long>(stage[D3DTSS_MINFILTER]),
                  static_cast<unsigned long>(stage[D3DTSS_MAGFILTER]),
                  static_cast<unsigned long long>(device->root->frame_number),
                  bounds);
    ReportCompositionDiagnostic(device->root, detail);
}

void ReportLateDrawDiagnostic(
    DeviceFacade* device,
    const re2dj::graphics::LegacyDrawCommand& command,
    const re2dj::graphics::LegacyFixedFunctionState& state,
    DWORD flags,
    DWORD vertex_type)
{
    // Runs on the draw path and scans the whole texture surface when it is
    // dirty, so it is off unless diagnostics were requested.
    if (!re2dj::platform::windows::AreGraphicsDrawDiagnosticsEnabled())
    {
        return;
    }
    if (device == nullptr || device->root == nullptr || command.vertices.empty())
    {
        return;
    }
    const SurfaceFacade* texture_surface = device->texture_stage_zero == nullptr
                                               ? nullptr
                                               : SurfaceFromTexture(device->texture_stage_zero);
    const auto& stage = device->state.texture_stage_states[0];
    const bool is_music_select_disc =
        texture_surface != nullptr &&
        (texture_surface->diagnostic_id == 279 || texture_surface->diagnostic_id == 387);
    if (is_music_select_disc)
    {
        constexpr std::uint32_t kMaximumMusicSelectDiscDiagnostics = 2048;
        if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
            ++device->root->music_select_disc_diagnostic_count >
                kMaximumMusicSelectDiscDiagnostics)
        {
            return;
        }

        std::string vertices;
        for (std::size_t index = 0; index < command.vertices.size(); ++index)
        {
            char vertex[256] = {};
            const auto& value = command.vertices[index];
            std::snprintf(vertex,
                          sizeof(vertex),
                          "v%zu=%.3f,%.3f,%.6f,%.6f,%.6f,%.6f,0x%08lx",
                          index,
                          value.x,
                          value.y,
                          value.z,
                          value.reciprocal_w,
                          value.texture_u,
                          value.texture_v,
                          static_cast<unsigned long>(value.diffuse_argb));
            if (!vertices.empty())
            {
                vertices += ";";
            }
            vertices += vertex;
        }

        D3DMATRIX texture_transform;
        re2dj::platform::windows::CopyFromCore(&texture_transform,
                                               device->state.transforms[kD3dTextureTransform0]);
        char detail[4096] = {};
        std::snprintf(
            detail,
            sizeof(detail),
            "MusicSelectDiscDraw:frame=%llu:texture=%lu:fvf=0x%08lx:topology=%u:vertices=%lu:"
            "cull=%lu:blend=%lu:srcblend=%lu:dstblend=%lu:zenable=%lu:zwrite=%lu:zfunc=%lu:"
            "texcoordindex=%lu:textransformflags=%lu:texmatrix=%.6f,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f:%s",
            static_cast<unsigned long long>(device->root->frame_number),
            static_cast<unsigned long>(texture_surface->diagnostic_id),
            static_cast<unsigned long>(vertex_type),
            command.topology == re2dj::graphics::PrimitiveTopology::kLineList
                ? 2U
                : command.topology == re2dj::graphics::PrimitiveTopology::kTriangleList ? 4U
                                                                                           : 5U,
            static_cast<unsigned long>(command.vertices.size()),
            static_cast<unsigned long>(
                device->state.render_states[D3DRENDERSTATE_CULLMODE]),
            static_cast<unsigned long>(
                device->state.render_states[D3DRENDERSTATE_ALPHABLENDENABLE]),
            static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_SRCBLEND]),
            static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_DESTBLEND]),
            static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_ZENABLE]),
            static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_ZWRITEENABLE]),
            static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_ZFUNC]),
            static_cast<unsigned long>(stage[kD3dTextureStageTexcoordIndex]),
            static_cast<unsigned long>(stage[kD3dTextureStageTransformFlags]),
            texture_transform._11,
            texture_transform._12,
            texture_transform._13,
            texture_transform._14,
            texture_transform._21,
            texture_transform._22,
            texture_transform._23,
            texture_transform._24,
            texture_transform._31,
            texture_transform._32,
            texture_transform._33,
            texture_transform._34,
            texture_transform._41,
            texture_transform._42,
            texture_transform._43,
            texture_transform._44,
            vertices.c_str());
        ReportCompositionDiagnostic(device->root, detail);
        return;
    }
    // Keep the trace bounded while retaining the later menu composition after
    // the high-volume attract/demo draws have filled the general budget.
    constexpr std::uint64_t kTargetFrame = 3000;
    constexpr std::uint32_t kMaximumLateDrawDiagnostics = 16384;
    constexpr std::uint32_t kMaximumLateDrawTargetDiagnostics = 4096;
    if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
        device->root->frame_number >= kTargetFrame)
    {
        if (++device->root->late_draw_target_diagnostic_count >
            kMaximumLateDrawTargetDiagnostics)
        {
            return;
        }
    }
    else if (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
             ++device->root->late_draw_diagnostic_count > kMaximumLateDrawDiagnostics)
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
    char detail[1536] = {};
    std::snprintf(detail,
                  sizeof(detail),
                  "LateDraw:frame=%llu:fvf=0x%08lx:texture=%lu:topology=%u:vertices=%lu:bounds=%.3f,%.3f,%.3f,%.3f:z=%.6f,%.6f:rhw=%.6f,%.6f:uv=%.6f,%.6f,%.6f,%.6f:diffuse=0x%08lx:flags=0x%08lx:blend=%lu:srcblend=%lu:dstblend=%lu:zenable=%lu:zwrite=%lu:zfunc=%lu:texsize=%lux%lu:key=%u:colorkey=%lu:alphatest=%lu:alpharef=%lu:alphafunc=%lu:alphaop=%lu:alphaarg1=%lu:alphaarg2=%lu:minfilter=%lu:magfilter=%lu:addressu=%lu:addressv=%lu:lighting=%lu:keylow=0x%04lx:keyhigh=0x%04lx:nonkey=%llu:nonzero=%llu:nonkeybbox=%lu,%lu,%lu,%lu:nonzerobbox=%lu,%lu,%lu,%lu:effectiveblend=%u:effectivesrcblend=%u:effectivedstblend=%u:effectivedepth=%u:%u:%u:fadecompat=%u",
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
                      device->state.render_states[D3DRENDERSTATE_ALPHABLENDENABLE]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_SRCBLEND]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_DESTBLEND]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_ZENABLE]),
                  static_cast<unsigned long>(
                      device->state.render_states[D3DRENDERSTATE_ZWRITEENABLE]),
                  static_cast<unsigned long>(device->state.render_states[D3DRENDERSTATE_ZFUNC]),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(texture_surface->width),
                  texture_surface == nullptr
                      ? 0UL
                      : static_cast<unsigned long>(texture_surface->height),
                  texture_surface != nullptr && texture_surface->has_source_blt_color_key ? 1U
                                                                                          : 0U,
                  static_cast<unsigned long>(
                      device->state.render_states[D3DRENDERSTATE_COLORKEYENABLE]),
                  static_cast<unsigned long>(
                      device->state.render_states[D3DRENDERSTATE_ALPHATESTENABLE]),
                  static_cast<unsigned long>(
                      device->state.render_states[D3DRENDERSTATE_ALPHAREF]),
                  static_cast<unsigned long>(
                      device->state.render_states[D3DRENDERSTATE_ALPHAFUNC]),
                  static_cast<unsigned long>(stage[D3DTSS_ALPHAOP]),
                  static_cast<unsigned long>(stage[D3DTSS_ALPHAARG1]),
                  static_cast<unsigned long>(stage[D3DTSS_ALPHAARG2]),
                  static_cast<unsigned long>(stage[D3DTSS_MINFILTER]),
                  static_cast<unsigned long>(stage[D3DTSS_MAGFILTER]),
                  static_cast<unsigned long>(stage[D3DTSS_ADDRESSU]),
                  static_cast<unsigned long>(stage[D3DTSS_ADDRESSV]),
                  static_cast<unsigned long>(
                      device->state.render_states[kD3dRenderStateLighting]),
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
    if ((surface.capabilities & DDSCAPS_PRIMARYSURFACE) != 0 && input != nullptr)
    {
        if (input->right <= input->left || input->bottom <= input->top)
        {
            return false;
        }
        output->x = 0;
        output->y = 0;
        output->width = static_cast<std::uint32_t>(input->right - input->left);
        output->height = static_cast<std::uint32_t>(input->bottom - input->top);
        return true;
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
                                                  destination->root->display.mode.width,
                                                  destination->root->display.mode.height,
                                                  &source_view,
                                                  &error))
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
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
    return re2dj::directx::BuildFixedFunctionState(device.state, state, error);
}

struct ViewportFacade
{
    IDirect3DViewport3 interface_value = {ViewportVtable()};
    volatile LONG references = 1;
    DWORD magic = kViewportMagic;
    RootFacade* root = nullptr;
    D3DVIEWPORT2 viewport = {};
};

void CopyMatrix(const re2dj::directx::D3dMatrix& source, re2dj::graphics::LegacyMatrix4x4* destination)
{
    destination->values = source.values;
}

bool BuildLegacyTransformState(const DeviceFacade& device,
                               re2dj::graphics::LegacyTransformState* transform,
                               std::string* error)
{
    if (transform == nullptr || error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "untransformed draw has no transform destination";
        }
        return false;
    }
    if (device.current_viewport != nullptr)
    {
        const ViewportFacade* const viewport =
            ViewportFromInterface(device.current_viewport);
        if (viewport->magic != kViewportMagic)
        {
            *error = "untransformed draw has an invalid viewport";
            return false;
        }
        re2dj::directx::D3dViewport2 source;
        re2dj::platform::windows::CopyToCore(&source, viewport->viewport);
        return re2dj::directx::BuildViewport2TransformState(device.state, source, transform, error);
    }
    // DirectX 7 sets its viewport on the device: the core's transform.
    return re2dj::directx::BuildTransformState(device.state, transform, error);
}

void ReportTransformDiagnostic(const DeviceFacade& device,
                               DWORD vertex_type,
                               const re2dj::graphics::LegacyTransformState& transform,
                               std::span<const std::byte> source,
                               std::size_t vertex_count,
                               const re2dj::graphics::LegacyDrawCommand& command)
{
    // Also on the draw path: it walks the untransformed vertex block before
    // formatting, so it follows the same switch.
    if (!re2dj::platform::windows::AreGraphicsDrawDiagnosticsEnabled())
    {
        return;
    }
    if (device.root == nullptr ||
        (!re2dj::platform::windows::AreCompleteDiagnosticsEnabled() &&
         ++device.root->transform_diagnostic_count > 128))
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

// Whether a vertex buffer belongs to this facade. The interface version the
// guest holds decides which table the object carries, so both the DirectX 6
// table and whichever table the device's root installs are ours.
bool IsVertexBufferOfDevice(const DeviceFacade* device, const IDirect3DVertexBuffer* buffer)
{
    if (device == nullptr || device->root == nullptr || buffer == nullptr)
    {
        return false;
    }
    return buffer->lpVtbl == VertexBufferVtable() ||
           (device->root->vertex_buffer_vtable != nullptr &&
            buffer->lpVtbl == device->root->vertex_buffer_vtable);
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
    re2dj::platform::windows::CopyFromCore(format, re2dj::directx::Rgb565Format());
}

// Gives a freshly created surface the interface version its root hands out.
// The object is identical either way; only the table the guest calls through
// differs.
void InstallSurfaceVtable(const RootFacade* root, SurfaceFacade* surface)
{
    if (root->surface_vtable != nullptr)
    {
        surface->interface_value.lpVtbl =
            const_cast<IDirectDrawSurface4Vtbl*>(root->surface_vtable);
    }
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
    surface->pitch = re2dj::directx::Rgb565Pitch(surface->width);

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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, qi_buf);
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectDraw4::GetCaps");
    bool valid = true;
    const auto fill = [&valid](DDCAPS* caps) {
        if (caps != nullptr)
        {
            if (caps->dwSize != sizeof(DDCAPS))
            {
                valid = false;
                return;
            }
            re2dj::platform::windows::CopyFromCore(caps, re2dj::directx::DirectDraw4Caps());
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirectDraw4::CreateSurface");
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
    // The pixel format decides whether a texture request can be served, and the
    // only 16-bit layout the shared surface backing provides is RGB565. Record
    // what the guest asked for so a rejection is attributable to the format
    // rather than to the request being unsupported in general.
    re2dj::platform::windows::WriteGraphicsTraceFormat(
        "re2dj:hle:CreateSurface:flags=0x%08lx:caps=0x%08lx:%lux%lu:"
        "back_buffers=%lu:pf_flags=0x%08lx:bpp=%lu:r=0x%08lx:g=0x%08lx:b=0x%08lx:"
        "a=0x%08lx",
        descriptor->dwFlags,
        descriptor->ddsCaps.dwCaps,
        static_cast<unsigned long>(descriptor->dwWidth),
        static_cast<unsigned long>(descriptor->dwHeight),
        static_cast<unsigned long>(descriptor->dwBackBufferCount),
        descriptor->ddpfPixelFormat.dwFlags,
        static_cast<unsigned long>(descriptor->ddpfPixelFormat.dwRGBBitCount),
        static_cast<unsigned long>(descriptor->ddpfPixelFormat.dwRBitMask),
        static_cast<unsigned long>(descriptor->ddpfPixelFormat.dwGBitMask),
        static_cast<unsigned long>(descriptor->ddpfPixelFormat.dwBBitMask),
        static_cast<unsigned long>(descriptor->ddpfPixelFormat.dwRGBAlphaBitMask));
    // How the guest presents is decided by the primary it creates, and it is
    // read here rather than at device creation because the render backend is
    // built on the first draw, which comes later. The shared core's plan
    // decides which surfaces are served and with what shape.
    re2dj::directx::DdSurfaceDesc2 request;
    std::memcpy(&request, descriptor, sizeof(request));
    const re2dj::directx::SurfacePlan plan = re2dj::directx::PlanCreateSurface(request, root->display);
    if (plan.retains_frames)
    {
        root->presentation_retains_frames = true;
    }
    if (plan.result != DD_OK)
    {
        return finish(static_cast<HRESULT>(plan.result));
    }
    // A facade for one planned surface. Surfaces with pixels get an RGB565
    // GDI backing and a texture identity; the depth surface carries only its
    // descriptor.
    const auto make_surface = [root](const re2dj::directx::SurfaceShape& shape) -> SurfaceFacade* {
        auto* const created = new (std::nothrow) SurfaceFacade;
        if (created == nullptr)
        {
            return nullptr;
        }
        created->root = root;
        created->width = shape.width;
        created->height = shape.height;
        created->bits_per_pixel = shape.bits_per_pixel;
        created->capabilities = shape.caps;
        created->diagnostic_id = AllocateSurfaceDiagnosticId(root);
        if (shape.has_pixels())
        {
            created->texture_identity = AllocateSurfaceIdentity(root);
            if (!CreateRgb565GdiBacking(created))
            {
                delete created;
                return nullptr;
            }
        }
        return created;
    };
    SurfaceFacade* const created = make_surface(plan.surface);
    if (created == nullptr)
    {
        return finish(DDERR_OUTOFMEMORY);
    }
    if (plan.has_back_buffer)
    {
        SurfaceFacade* const back = make_surface(plan.back_buffer);
        if (back == nullptr)
        {
            DestroyGdiBacking(created);
            delete created;
            return finish(DDERR_OUTOFMEMORY);
        }
        created->attached_back_buffer = back;
        InstallSurfaceVtable(root, back);
        AddRootReference(root);
    }
    InstallSurfaceVtable(root, created);
    AddRootReference(root);
    *surface = &created->interface_value;
    if (plan.surface.kind == re2dj::directx::SurfaceKind::kTexture)
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime,
                                                  kCreateTextureSurfaceMessage);
    }
    return finish(DD_OK, created);
}

HRESULT WINAPI RootSetCooperativeLevel(IDirectDraw4* self, HWND window, DWORD flags)
{
    char coop_buf[80] = {};
    std::snprintf(coop_buf, sizeof(coop_buf), "re2dj:hle:IDirectDraw4::SetCooperativeLevel hwnd=0x%08x flags=0x%08x",
                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(window)), static_cast<unsigned>(flags));
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, coop_buf);
    RootFacade* const root = RootFromDirectDraw(self);
    // The host's policy is the Win32 window mode: the guest's window becomes
    // the presentation window at the display's size.
    const HRESULT result = static_cast<HRESULT>(re2dj::directx::SetCooperativeLevel(
        &root->display,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(window)),
        flags,
        [window](std::uint32_t, const re2dj::directx::DisplayMode& mode) {
            return ApplyRe2djWindowMode(window, mode.width, mode.height);
        }));
    if (result != DD_OK)
    {
        return result;
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, mode_buf);
    RootFacade* const root = RootFromDirectDraw(self);
    return static_cast<HRESULT>(
        re2dj::directx::SetDisplayMode(&root->display, {width, height, bits_per_pixel}));
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kFindDeviceMessage);
    if (search == nullptr || result == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    // The shared core decides, so the Linux facade answers alike.
    re2dj::directx::D3dFindDeviceSearch core_search;
    re2dj::directx::D3dFindDeviceResult core_result;
    re2dj::platform::windows::CopyToCore(&core_search, *search);
    re2dj::platform::windows::CopyToCore(&core_result, *result);
    const HRESULT found = static_cast<HRESULT>(re2dj::directx::FindDevice(core_search, &core_result));
    if (found == DD_OK)
    {
        re2dj::platform::windows::CopyFromCore(result, core_result);
    }
    return found;
}

HRESULT WINAPI D3dCreateDevice(IDirect3D3* self,
                               REFCLSID device_class,
                               IDirectDrawSurface4* render_target,
                               IDirect3DDevice3** device,
                               IUnknown* outer)
{
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kCreateDeviceMessage);
    if (device == nullptr || render_target == nullptr || outer != nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *device = nullptr;
    // The device classes and the render target follow the shared core: the
    // enumeration's three devices all land on the same implementation, which
    // renders into a DDSCAPS_3DDEVICE surface.
    re2dj::directx::Guid core_class;
    re2dj::platform::windows::CopyToCore(&core_class, device_class);
    SurfaceFacade* const target = SurfaceFromInterface(render_target);
    if (!re2dj::directx::IsEnumeratedDevice(core_class) || target->magic != kSurfaceMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    const HRESULT checked = static_cast<HRESULT>(
        re2dj::directx::CheckCreateDevice(core_class, target->capabilities));
    if (checked != DD_OK)
    {
        return checked;
    }
    auto* const facade = new (std::nothrow) DeviceFacade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->root = RootFromDirect3d(self);
    facade->render_target = target;
    if (facade->root != nullptr)
    {
        facade->root->presentation_surface = target;
    }
    facade->state = re2dj::directx::InitialDeviceState();
    if (facade->root->device_vtable != nullptr)
    {
        facade->interface_value.lpVtbl =
            const_cast<IDirect3DDevice3Vtbl*>(facade->root->device_vtable);
    }
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
    re2dj::directx::Guid core_class;
    re2dj::platform::windows::CopyToCore(&core_class, device_class);
    if (callback == nullptr || re2dj::directx::CheckEnumZBufferFormats3(core_class) != DD_OK)
    {
        return DDERR_INVALIDPARAMS;
    }
    DDPIXELFORMAT format;
    re2dj::platform::windows::CopyFromCore(&format, re2dj::directx::Direct3D3DepthFormat());
    callback(&format, context);
    return DD_OK;
}

HRESULT WINAPI D3dCreateVertexBuffer(IDirect3D3* self,
                                     D3DVERTEXBUFFERDESC* descriptor,
                                     IDirect3DVertexBuffer** vertex_buffer,
                                     DWORD flags,
                                     IUnknown* outer)
{
    if (outer != nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    re2dj::directx::D3dVertexBufferDesc core_descriptor;
    if (descriptor != nullptr)
    {
        core_descriptor.size = descriptor->dwSize;
        core_descriptor.caps = descriptor->dwCaps;
        core_descriptor.fvf = descriptor->dwFVF;
        core_descriptor.vertex_count = descriptor->dwNumVertices;
    }
    std::uint32_t stride = 0;
    const std::uint32_t checked = re2dj::directx::CheckCreateVertexBuffer(
        vertex_buffer != nullptr, descriptor != nullptr, core_descriptor, &stride);
    if (vertex_buffer != nullptr)
    {
        *vertex_buffer = nullptr;
    }
    if (checked != DD_OK)
    {
        return static_cast<HRESULT>(checked);
    }
    char message[160] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:hle:IDirect3D3::CreateVertexBuffer:caps=0x%08lx:fvf=0x%08lx:vertices=%lu:flags=0x%08lx",
                  descriptor->dwCaps,
                  descriptor->dwFVF,
                  static_cast<unsigned long>(descriptor->dwNumVertices),
                  flags);
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, message);
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
    if (facade->root->vertex_buffer_vtable != nullptr)
    {
        facade->interface_value.lpVtbl =
            const_cast<IDirect3DVertexBufferVtbl*>(facade->root->vertex_buffer_vtable);
    }
    AddRootReference(facade->root);
    *vertex_buffer = &facade->interface_value;
    char result_message[160] = {};
    std::snprintf(result_message,
                  sizeof(result_message),
                  "re2dj:hle:IDirect3D3::CreateVertexBuffer:result=%p:vtable=%p",
                  static_cast<void*>(*vertex_buffer),
                  static_cast<void*>((*vertex_buffer)->lpVtbl));
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, result_message);
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
        SurfaceFacade* const depth = surface->attached_depth_buffer;
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
        if (depth != nullptr)
        {
            SurfaceRelease(&depth->interface_value);
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
        if ((surface->capabilities & DDSCAPS_PRIMARYSURFACE) != 0)
        {
            if (surface->root->render_backend != nullptr)
            {
                std::string error;
                const bool presented = PresentAndRecordCost(surface->root, &error);
                Re2djExitIfWindowClosed(surface->root->window);
                if (!presented)
                {
                    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
                    return finish(DDERR_GENERIC);
                }
            }
            else
            {
                Re2djExitIfWindowClosed(surface->root->window);
            }
            ++surface->root->frame_number;
            RecordPresentedFrame(surface->root);
            return finish(DD_OK);
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
    // Filling the surface the guest presents from is a display-layer screen
    // clear. Only a fill of the whole surface maps onto a target clear; a
    // partial one is a region update, and the memory copy above carries it.
    RootFacade* const root = surface->root;
    if (root != nullptr && surface == root->presentation_surface &&
        rectangle.left == full.left &&
        rectangle.top == full.top && rectangle.right == full.right &&
        rectangle.bottom == full.bottom)
    {
        std::string clear_error;
        if (!RequestRenderTargetClear(root, color, &clear_error))
        {
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
            return finish(DDERR_GENERIC);
        }
    }
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
    if ((destination_surface->capabilities & DDSCAPS_PRIMARYSURFACE) != 0)
    {
        if (destination_surface->root->render_backend != nullptr)
        {
            std::string error;
            const bool presented = PresentAndRecordCost(destination_surface->root, &error);
            Re2djExitIfWindowClosed(destination_surface->root->window);
            if (!presented)
            {
                re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
                return finish(DDERR_GENERIC);
            }
        }
        else
        {
            Re2djExitIfWindowClosed(destination_surface->root->window);
        }
        ++destination_surface->root->frame_number;
        RecordPresentedFrame(destination_surface->root);
        return finish(DD_OK);
    }
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
    // One summary per presented frame, with its own budget: the Flip
    // diagnostic budget is far too small to cover a boot sequence, and this is
    // the record that says whether the guest drew anything for this frame.
    {
        RootFacade* const root = surface->root;
        constexpr std::uint32_t kMaximumFrameDrawSummaries = 900;
        if (re2dj::platform::windows::AreCompleteDiagnosticsEnabled() ||
            root->frame_draw_summary_count < kMaximumFrameDrawSummaries)
        {
            ++root->frame_draw_summary_count;
            LARGE_INTEGER now = {};
            LARGE_INTEGER frequency = {};
            double milliseconds = 0.0;
            if (QueryPerformanceCounter(&now) != FALSE &&
                QueryPerformanceFrequency(&frequency) != FALSE && frequency.QuadPart > 0)
            {
                if (root->frame_summary_previous_counter.QuadPart != 0)
                {
                    milliseconds =
                        1000.0 *
                        static_cast<double>(now.QuadPart -
                                            root->frame_summary_previous_counter.QuadPart) /
                        static_cast<double>(frequency.QuadPart);
                }
                root->frame_summary_previous_counter = now;
            }
            char detail[192] = {};
            std::snprintf(detail,
                          sizeof(detail),
                          "FrameDraws:frame=%llu:ms=%.2f:draws=%u:vertices=%u:textured=%u"
                          ":diffuse=0x%08x:last=0x%08x",
                          static_cast<unsigned long long>(root->frame_number),
                          milliseconds,
                          root->frame_draw_calls,
                          root->frame_draw_vertices,
                          root->frame_textured_draw_calls,
                          root->frame_first_diffuse,
                          root->frame_last_diffuse);
            ReportCompositionDiagnostic(root, detail);
        }
        root->frame_draw_calls = 0;
        root->frame_draw_vertices = 0;
        root->frame_textured_draw_calls = 0;
        root->frame_first_diffuse = 0;
        root->frame_first_diffuse_seen = false;
        root->frame_last_diffuse = 0;
    }
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
        const bool presented = PresentAndRecordCost(surface->root, &error);
        Re2djExitIfWindowClosed(surface->root->window);
        if (!presented)
        {
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
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

HRESULT WINAPI SurfaceAddAttachedSurface(IDirectDrawSurface4* self,
                                         IDirectDrawSurface4* attachment)
{
    if (attachment == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    SurfaceFacade* const facade = SurfaceFromInterface(self);
    SurfaceFacade* const attached = SurfaceFromInterface(attachment);
    if (facade->magic != kSurfaceMagic || attached->magic != kSurfaceMagic ||
        attached->root != facade->root)
    {
        return DDERR_INVALIDOBJECT;
    }
    // The only attachment a guest makes to a render target here is its depth
    // buffer; a back buffer arrives already attached from CreateSurface.
    re2dj::directx::SurfaceShape attached_shape;
    attached_shape.caps = attached->capabilities;
    const HRESULT attachable = static_cast<HRESULT>(re2dj::directx::CheckAttachment(attached_shape));
    if (attachable != DD_OK)
    {
        return attachable;
    }
    if (facade->attached_depth_buffer == attached)
    {
        return DD_OK;
    }
    SurfaceAddRef(attachment);
    if (facade->attached_depth_buffer != nullptr)
    {
        SurfaceRelease(&facade->attached_depth_buffer->interface_value);
    }
    facade->attached_depth_buffer = attached;
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
    SurfaceFacade* found = nullptr;
    re2dj::directx::DdsCaps2 query;
    std::memcpy(&query, capabilities, sizeof(query));
    switch (re2dj::directx::QueryAttachment(query))
    {
    case re2dj::directx::AttachmentQuery::kBackBuffer:
        found = facade->attached_back_buffer;
        break;
    case re2dj::directx::AttachmentQuery::kDepth:
        found = facade->attached_depth_buffer;
        break;
    case re2dj::directx::AttachmentQuery::kNone:
        break;
    }
    if (found == nullptr)
    {
        return DDERR_NOTFOUND;
    }
    *surface = &found->interface_value;
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
    re2dj::directx::SurfaceShape shape;
    shape.width = surface->width;
    shape.height = surface->height;
    shape.caps = surface->capabilities;
    re2dj::platform::windows::CopyFromCore(descriptor, re2dj::directx::SurfaceDescription(shape, surface->pitch));
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
    const auto checked = static_cast<HRESULT>(
        re2dj::directx::CheckGetDc(surface->bitmap_dc != nullptr, surface->dc_acquired));
    if (checked != DD_OK)
    {
        return finish(checked);
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
    const auto checked = static_cast<HRESULT>(
        re2dj::directx::CheckReleaseDc(surface->dc_acquired, dc != nullptr && dc == surface->bitmap_dc));
    if (checked != DD_OK)
    {
        return finish(checked);
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
    const auto checked =
        static_cast<HRESULT>(re2dj::directx::CheckSetColorKey(flags, color_key != nullptr));
    if (checked != DD_OK)
    {
        return checked;
    }
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    surface->source_blt_color_key = *color_key;
    surface->has_source_blt_color_key = true;
    return DD_OK;
}

// Hands the guest the surface's own pixels. The 1st SE guest uploads through
// GetDC and Blt and never reaches here; the 4th guest locks its textures and
// writes them directly, so the memory the surface already owns is what it gets.
// A sub-rectangle lock returns the address of that rectangle's first pixel, as
// DirectDraw does, and the pitch stays the whole surface's pitch. The surface
// the guest presents from has its picture on the backend's render target, so
// the locked area is read back from there first (4th's F1 screen draws into
// the back buffer this way), and Unlock puts it back.
HRESULT WINAPI SurfaceLock(IDirectDrawSurface4* self,
                           RECT* rect,
                           DDSURFACEDESC2* descriptor,
                           DWORD flags,
                           HANDLE event)
{
    (void)flags;
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    // The shared core decides, so the Linux facade answers alike.
    re2dj::directx::SurfaceShape shape;
    shape.width = surface->width;
    shape.height = surface->height;
    shape.bits_per_pixel = surface->bits_per_pixel;
    shape.caps = surface->capabilities;
    re2dj::directx::SurfaceRect core_rect;
    if (rect != nullptr)
    {
        core_rect = {rect->left, rect->top, rect->right, rect->bottom};
    }
    const re2dj::directx::SurfaceLockPlan plan = re2dj::directx::PlanLock(
        shape, surface->pitch, surface->magic == kSurfaceMagic && surface->pixels != nullptr,
        descriptor != nullptr && descriptor->dwSize == sizeof(DDSURFACEDESC2), event != nullptr,
        rect == nullptr ? nullptr : &core_rect);
    if (plan.result != DD_OK)
    {
        return static_cast<HRESULT>(plan.result);
    }
    surface->lock_x = rect == nullptr ? 0 : static_cast<DWORD>(rect->left);
    surface->lock_y = rect == nullptr ? 0 : static_cast<DWORD>(rect->top);
    surface->lock_width = plan.description.width;
    surface->lock_height = plan.description.height;
    unsigned char* const first = static_cast<unsigned char*>(surface->pixels) + plan.offset;
    if (surface->root != nullptr && surface == surface->root->presentation_surface &&
        surface->root->render_backend != nullptr)
    {
        const std::size_t span_bytes =
            static_cast<std::size_t>(surface->pitch) * (surface->lock_height - 1) + surface->lock_width * 2;
        std::string error;
        if (!surface->root->render_backend->ReadRenderTarget(surface->lock_x, surface->lock_y, surface->lock_width,
                                                             surface->lock_height, std::span(first, span_bytes),
                                                             surface->pitch, &error))
        {
            return DDERR_GENERIC;
        }
    }
    surface->locked = true;
    re2dj::platform::windows::CopyFromCore(descriptor, plan.description);
    descriptor->lpSurface = first;
    return DD_OK;
}

HRESULT WINAPI SurfaceUnlock(IDirectDrawSurface4* self, RECT*)
{
    SurfaceFacade* const surface = SurfaceFromInterface(self);
    if (surface->magic != kSurfaceMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    // The guest may have written anything into the pixels, so the texture the
    // backend caches for this surface is stale from here on.
    MarkSurfaceDirty(surface);
    const bool was_locked = surface->locked;
    surface->locked = false;
    if (was_locked && surface->root != nullptr && surface == surface->root->presentation_surface &&
        surface->root->render_backend != nullptr)
    {
        const unsigned char* const first = static_cast<const unsigned char*>(surface->pixels) +
                                           static_cast<std::size_t>(surface->lock_y) * surface->pitch +
                                           static_cast<std::size_t>(surface->lock_x) * 2;
        const std::size_t span_bytes =
            static_cast<std::size_t>(surface->pitch) * (surface->lock_height - 1) + surface->lock_width * 2;
        std::string error;
        if (!surface->root->render_backend->WriteRenderTarget(surface->lock_x, surface->lock_y,
                                                              surface->lock_width, surface->lock_height,
                                                              std::span(first, span_bytes), surface->pitch,
                                                              &error))
        {
            return DDERR_GENERIC;
        }
    }
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
    // The shared core decides, so the Linux facade answers alike.
    re2dj::directx::D3dDeviceDesc6 core_hardware;
    re2dj::directx::D3dDeviceDesc6 core_software;
    if (hardware != nullptr)
    {
        re2dj::platform::windows::CopyToCore(&core_hardware, *hardware);
    }
    if (software != nullptr)
    {
        re2dj::platform::windows::CopyToCore(&core_software, *software);
    }
    const HRESULT answer = static_cast<HRESULT>(re2dj::directx::GetDevice3Caps(
        hardware == nullptr ? nullptr : &core_hardware, software == nullptr ? nullptr : &core_software));
    if (hardware != nullptr && hardware->dwSize == sizeof(D3DDEVICEDESC))
    {
        re2dj::platform::windows::CopyFromCore(hardware, core_hardware);
    }
    if (answer == DD_OK && software != nullptr)
    {
        re2dj::platform::windows::CopyFromCore(software, core_software);
    }
    return answer;
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
    return static_cast<HRESULT>(re2dj::directx::BeginScene(DeviceFromInterface(self)->state));
}

HRESULT WINAPI DeviceEndScene(IDirect3DDevice3* self)
{
    return static_cast<HRESULT>(re2dj::directx::EndScene(DeviceFromInterface(self)->state));
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
    if (value == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    std::uint32_t core_value = 0;
    const auto result = static_cast<HRESULT>(re2dj::directx::GetRenderState(
        DeviceFromInterface(self)->state, static_cast<std::uint32_t>(state), &core_value));
    if (result == DD_OK)
    {
        *value = core_value;
    }
    return result;
}

HRESULT WINAPI DeviceSetRenderState(IDirect3DDevice3* self,
                                    D3DRENDERSTATETYPE state,
                                    DWORD value)
{
    const unsigned state_index = static_cast<unsigned>(state);
    DeviceFacade* const device = DeviceFromInterface(self);
    std::uint32_t previous = 0;
    re2dj::directx::GetRenderState(device->state, state_index, &previous);
    const auto result = static_cast<HRESULT>(re2dj::directx::SetRenderState(device->state, state_index, value));
    if (result != DD_OK)
    {
        return result;
    }
    std::uint8_t& reports = device->render_state_reports[state_index];
    if (reports < 8 && (reports == 0 || previous != value))
    {
        char message[128] = {};
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:hle:render-state:state=%u:value=0x%08x",
                      state_index,
                      static_cast<unsigned>(value));
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, message);
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
    if (value == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    std::uint32_t core_value = 0;
    const auto result = static_cast<HRESULT>(re2dj::directx::GetLightState(
        DeviceFromInterface(self)->state, static_cast<std::uint32_t>(state), &core_value));
    if (result == DD_OK)
    {
        *value = core_value;
    }
    return result;
}

HRESULT WINAPI DeviceSetLightState(IDirect3DDevice3* self,
                                   D3DLIGHTSTATETYPE state,
                                   DWORD value)
{
    return static_cast<HRESULT>(re2dj::directx::SetLightState(
        DeviceFromInterface(self)->state, static_cast<std::uint32_t>(state), value));
}

HRESULT WINAPI DeviceSetTransform(IDirect3DDevice3* self,
                                  D3DTRANSFORMSTATETYPE state,
                                  D3DMATRIX* matrix)
{
    if (matrix == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    re2dj::directx::D3dMatrix core_matrix;
    re2dj::platform::windows::CopyToCore(&core_matrix, *matrix);
    return static_cast<HRESULT>(re2dj::directx::SetTransform(
        DeviceFromInterface(self)->state, static_cast<std::uint32_t>(state), core_matrix));
}

HRESULT WINAPI DeviceGetTransform(IDirect3DDevice3* self,
                                  D3DTRANSFORMSTATETYPE state,
                                  D3DMATRIX* matrix)
{
    if (matrix == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    re2dj::directx::D3dMatrix core_matrix;
    const auto result = static_cast<HRESULT>(re2dj::directx::GetTransform(
        DeviceFromInterface(self)->state, static_cast<std::uint32_t>(state), &core_matrix));
    if (result == DD_OK)
    {
        re2dj::platform::windows::CopyFromCore(matrix, core_matrix);
    }
    return result;
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
    if (value == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    std::uint32_t core_value = 0;
    const auto result = static_cast<HRESULT>(re2dj::directx::GetTextureStageState(
        DeviceFromInterface(self)->state, stage, static_cast<std::uint32_t>(state), &core_value));
    if (result == DD_OK)
    {
        *value = core_value;
    }
    return result;
}

HRESULT WINAPI DeviceSetTextureStageState(IDirect3DDevice3* self,
                                          DWORD stage,
                                          D3DTEXTURESTAGESTATETYPE state,
                                          DWORD value)
{
    const unsigned state_index = static_cast<unsigned>(state);
    DeviceFacade* const device = DeviceFromInterface(self);
    std::uint32_t previous = 0;
    re2dj::directx::GetTextureStageState(device->state, stage, state_index, &previous);
    const auto result = static_cast<HRESULT>(
        re2dj::directx::SetTextureStageState(device->state, stage, state_index, value));
    if (result != DD_OK)
    {
        return result;
    }
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
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, message);
        ++reports;
    }
    return DD_OK;
}

// Counted once per draw, at the top of the funnel every draw entry point
// reaches, and before validation so a draw the facade rejects still shows as an
// attempt. The vertex-buffer entry points forward here rather than drawing
// themselves, so counting them separately would double every VB draw. The
// totals are reported once per presented frame.
void CountFrameDraw(DeviceFacade* device, DWORD vertex_count)
{
    if (device == nullptr || device->root == nullptr)
    {
        return;
    }
    ++device->root->frame_draw_calls;
    device->root->frame_draw_vertices += vertex_count;
    if (device->texture_stage_zero != nullptr)
    {
        ++device->root->frame_textured_draw_calls;
    }
}

// The frame's first and last vertex colour. A guest that fades a full-screen
// quad does it by modulating this, so the sequence of these values across
// frames is the fade curve it actually asked for; the last one is recorded
// separately because a fade overlay is usually drawn after the scene.
void RecordFrameDiffuse(RootFacade* root, const re2dj::graphics::LegacyDrawCommand& command)
{
    if (root == nullptr || command.vertices.empty())
    {
        return;
    }
    const std::uint32_t diffuse = command.vertices.front().diffuse_argb;
    if (!root->frame_first_diffuse_seen)
    {
        root->frame_first_diffuse = diffuse;
        root->frame_first_diffuse_seen = true;
    }
    root->frame_last_diffuse = diffuse;
}

HRESULT WINAPI DeviceDrawPrimitive(IDirect3DDevice3* self,
                                   D3DPRIMITIVETYPE primitive,
                                   DWORD vertex_type,
                                   void* vertices,
                                   DWORD vertex_count,
                                   DWORD flags)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    CountFrameDraw(device, vertex_count);
    const re2dj::directx::DrawPlan plan = re2dj::directx::PlanDrawPrimitive(
        static_cast<std::uint32_t>(primitive), vertex_type, vertex_count, flags);
    const bool is_transformed = plan.transformed;
    const std::size_t vertex_stride = plan.vertex_stride;
    if (plan.result != DD_OK || vertices == nullptr)
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
    const re2dj::graphics::PrimitiveTopology topology = plan.topology;
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
    RecordFrameDiffuse(device->root, command);

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
        // Sampled on both sides of backend creation because that is where SDL
        // initializes, which makes it the first suspect for raising the
        // resolution the guest depends on without asking for it.
        re2dj::platform::windows::NoteTimerResolution("pre-backend");
        auto* const backend = new (std::nothrow) re2dj::graphics::Sdl3OpenGlBackend;
        const re2dj::graphics::Sdl3OpenGlWindowConfig window_config = {
            root->window,
            root->display.mode.width,
            root->display.mode.height,
            "re2DJ",
            re2dj::platform::windows::AreGraphicsDrawDiagnosticsEnabled(),
            root->presentation_retains_frames,
            re2dj::platform::windows::SelectedPresentSync()};
        const bool input_suspended = SuspendRe2djGuestWindowInput(root->window);
        const bool backend_initialized =
            input_suspended && backend != nullptr && backend->Initialize(window_config, &error);
        const bool input_restored =
            input_suspended && EnsureRe2djGuestWindowInput(root->window);
        if (!backend_initialized || !input_restored ||
            !ApplyRe2djWindowMode(root->window, root->display.mode.width, root->display.mode.height))
        {
            delete backend;
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
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
        re2dj::platform::windows::InstallProcessOsd(backend);
        // The driver can refuse the requested interval, so the record is the
        // value that actually applied. This is the only place the present
        // policy becomes observable in a detached product run.
        re2dj::platform::windows::WriteGraphicsTraceFormat(
            "re2dj:hle:present-sync:requested=%u:applied_interval=%d",
            static_cast<unsigned>(window_config.present_sync),
            backend->applied_swap_interval());
        re2dj::platform::windows::NoteTimerResolution("post-backend");
    }
    if (root->pending_render_target_clear)
    {
        const std::uint16_t color = root->pending_render_target_clear_color;
        if (!root->render_backend->ClearRenderTarget(color, &error))
        {
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
            ReportDrawDiagnostic(device,
                                 primitive,
                                 vertex_type,
                                 vertex_count,
                                 flags,
                                 DDERR_GENERIC,
                                 "pending-target-clear");
            return DDERR_GENERIC;
        }
        root->pending_render_target_clear = false;
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
    if (state_built)
    {
        re2dj::directx::ApplyFadeCompatibility(device->state,
                                               command,
                                               texture != nullptr,
                                               root->display.mode.width,
                                               root->display.mode.height,
                                               &fixed_function_state);
    }
    const bool drawn = state_built && root->render_backend->Draw(command,
                                                                 fixed_function_state,
                                                                 root->display.mode.width,
                                                                 root->display.mode.height,
                                                                 texture,
                                                                 &error);
    if (!drawn)
    {
        if (!device->draw_failure_reported)
        {
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
            char message[256] = {};
            std::snprintf(message,
                          sizeof(message),
                          "re2dj:hle:draw-failure:%s",
                          error.c_str());
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, message);
            device->draw_failure_reported = true;
        }
        ReportDrawDiagnostic(device,
                             primitive,
                             vertex_type,
                             vertex_count,
                             flags,
                             DDERR_GENERIC,
                             error.c_str(),
                             &command);
        return DDERR_GENERIC;
    }
    if (!device->draw_success_reported)
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kDrawPrimitiveMessage);
        device->draw_success_reported = true;
    }
    ReportDrawDiagnostic(
        device, primitive, vertex_type, vertex_count, flags, DD_OK, "success", &command);
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
        !IsVertexBufferOfDevice(device, vertex_buffer))
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

HRESULT WINAPI DeviceDrawPrimitiveVB(IDirect3DDevice3* self,
                                     D3DPRIMITIVETYPE primitive,
                                     IDirect3DVertexBuffer* vertex_buffer,
                                     DWORD start_vertex,
                                     DWORD vertex_count,
                                     DWORD flags)
{
    DeviceFacade* const device = DeviceFromInterface(self);
    if (vertex_buffer == nullptr || vertex_count == 0 ||
        IsBadReadPtr(vertex_buffer, sizeof(*vertex_buffer)) != FALSE ||
        !IsVertexBufferOfDevice(device, vertex_buffer))
    {
        ReportDrawDiagnostic(
            device, primitive, 0, vertex_count, flags, DDERR_INVALIDPARAMS, "invalid-vb");
        return DDERR_INVALIDPARAMS;
    }
    VertexBufferFacade* const facade = VertexBufferFromInterface(vertex_buffer);
    if (IsBadReadPtr(facade, sizeof(*facade)) != FALSE ||
        facade->magic != kVertexBufferMagic || facade->root != device->root ||
        facade->buffer == nullptr)
    {
        ReportDrawDiagnostic(
            device, primitive, 0, vertex_count, flags, DDERR_INVALIDPARAMS, "foreign-vb");
        return DDERR_INVALIDPARAMS;
    }
    const std::uint32_t checked = re2dj::directx::CheckDrawPrimitiveVB(
        vertex_count, facade->buffer->locked(), start_vertex, facade->descriptor.vertex_count);
    if (checked != DD_OK)
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             facade->descriptor.fvf,
                             vertex_count,
                             flags,
                             static_cast<HRESULT>(checked),
                             checked == D3DERR_VERTEXBUFFERLOCKED ? "locked-vb" : "vb-range");
        return static_cast<HRESULT>(checked);
    }
    const std::span<const std::byte> vertices = facade->buffer->vertices();
    const std::size_t stride = facade->buffer->stride();
    const std::size_t offset = static_cast<std::size_t>(start_vertex) * stride;
    if (stride == 0 || offset > vertices.size())
    {
        ReportDrawDiagnostic(device,
                             primitive,
                             facade->descriptor.fvf,
                             vertex_count,
                             flags,
                             DDERR_INVALIDPARAMS,
                             "vb-stride");
        return DDERR_INVALIDPARAMS;
    }
    // The draw path takes a plain vertex array, and a vertex buffer is exactly
    // that once the starting offset is applied.
    return DeviceDrawPrimitive(
        self,
        primitive,
        facade->descriptor.fvf,
        const_cast<std::byte*>(vertices.data()) + offset,
        vertex_count,
        flags);
}

HRESULT WINAPI DeviceSetRenderTarget(IDirect3DDevice3* self,
                                     IDirectDrawSurface4* surface,
                                     DWORD flags)
{
    (void)flags;
    DeviceFacade* const device = DeviceFromInterface(self);
    if (surface == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    SurfaceFacade* const target = SurfaceFromInterface(surface);
    if (target->magic != kSurfaceMagic || target->root != device->root)
    {
        return DDERR_INVALIDOBJECT;
    }
    if (target == device->render_target)
    {
        return DD_OK;
    }
    SurfaceAddRef(surface);
    if (device->render_target != nullptr)
    {
        SurfaceRelease(&device->render_target->interface_value);
    }
    device->render_target = target;
    if (device->root != nullptr)
    {
        device->root->presentation_surface = target;
    }
    return DD_OK;
}

HRESULT WINAPI DeviceGetRenderTarget(IDirect3DDevice3* self, IDirectDrawSurface4** surface)
{
    if (surface == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *surface = nullptr;
    DeviceFacade* const device = DeviceFromInterface(self);
    if (device->render_target == nullptr)
    {
        return DDERR_NOTFOUND;
    }
    *surface = &device->render_target->interface_value;
    SurfaceAddRef(*surface);
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, entry_message);
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
    const std::uint32_t checked = re2dj::directx::CheckLockVertexBuffer(
        true, facade->buffer->locked(), static_cast<std::uint32_t>(facade->buffer->vertices().size()));
    if (checked != DD_OK)
    {
        return static_cast<HRESULT>(checked);
    }
    std::span<std::byte> vertices = facade->buffer->Lock();
    char message[128] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:hle:IDirect3DVertexBuffer::Lock:success:bytes=%lu:output=%p:flags=0x%08lx",
                  static_cast<unsigned long>(vertices.size()),
                  static_cast<void*>(vertices.data()),
                  flags);
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, message);
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
    if (re2dj::directx::CheckUnlockVertexBuffer(facade->buffer->locked()) != DD_OK || !facade->buffer->Unlock())
    {
        re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirect3DVertexBuffer::Unlock:not-locked");
        return DDERR_NOTLOCKED;
    }
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirect3DVertexBuffer::Unlock");
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirect3DVertexBuffer::ProcessVertices");
    return E_NOTIMPL;
}

HRESULT WINAPI VbGetVertexBufferDesc(IDirect3DVertexBuffer* self, D3DVERTEXBUFFERDESC* descriptor)
{
    if (re2dj::directx::CheckGetVertexBufferDesc(descriptor != nullptr,
                                                 descriptor == nullptr ? 0U : descriptor->dwSize) != DD_OK)
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
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirect3DVertexBuffer::GetVertexBufferDesc");
    return DD_OK;
}

HRESULT WINAPI VbOptimize(IDirect3DVertexBuffer* self, IDirect3DDevice3*, DWORD)
{
    VertexBufferFacade* const facade = VertexBufferFromInterface(self);
    if (facade->magic != kVertexBufferMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, "re2dj:hle:IDirect3DVertexBuffer::Optimize");
    return E_NOTIMPL;
}

}  // namespace

namespace re2dj::platform::windows
{

const IDirectDraw4Vtbl* LegacyDirectDrawVtable()
{
    return DirectDrawVtable();
}

const IDirectDrawSurface4Vtbl* LegacyDirectDrawSurfaceVtable()
{
    return SurfaceVtable();
}

const IDirect3D3Vtbl* LegacyDirect3DVtable()
{
    return Direct3dVtable();
}

const IDirect3DDevice3Vtbl* LegacyDirect3DDeviceVtable()
{
    return DeviceVtable();
}

const IDirect3DVertexBufferVtbl* LegacyDirect3DVertexBufferVtable()
{
    return VertexBufferVtable();
}

HRESULT CreateLegacyDirectDrawRoot(const LegacyFacadeVtables& vtables, IDirectDraw4** root)
{
    if (root == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *root = nullptr;
    auto* const facade = new (std::nothrow) RootFacade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->surface_vtable = vtables.surface;
    facade->device_vtable = vtables.device;
    facade->vertex_buffer_vtable = vtables.vertex_buffer;
    *root = &facade->direct_draw;
    return DD_OK;
}

void SetLegacyDirectDrawVtable(IDirectDraw4* root, const IDirectDraw4Vtbl* vtable)
{
    if (root == nullptr || vtable == nullptr)
    {
        return;
    }
    root->lpVtbl = const_cast<IDirectDraw4Vtbl*>(vtable);
}

const re2dj::directx::DirectDrawDisplay* LegacyRootDisplay(IDirectDraw4* root)
{
    return root == nullptr ? nullptr : &RootFromDirectDraw(root)->display;
}

IDirect3D3* LegacyDirect3DOfRoot(IDirectDraw4* root)
{
    if (root == nullptr)
    {
        return nullptr;
    }
    return &RootFromDirectDraw(root)->direct3d;
}

IDirect3DTexture2* LegacyTextureOfSurface(IDirectDrawSurface4* surface)
{
    if (surface == nullptr)
    {
        return nullptr;
    }
    SurfaceFacade* const facade = SurfaceFromInterface(surface);
    if (facade->magic != kSurfaceMagic)
    {
        return nullptr;
    }
    return &facade->texture_interface;
}

IDirectDrawSurface4* LegacySurfaceOfTexture(IDirect3DTexture2* texture)
{
    if (texture == nullptr)
    {
        return nullptr;
    }
    SurfaceFacade* const facade = SurfaceFromTexture(texture);
    if (facade->magic != kSurfaceMagic)
    {
        return nullptr;
    }
    return &facade->interface_value;
}

HRESULT LegacyDeviceSetViewport(IDirect3DDevice3* device, const LegacyViewportState& viewport)
{
    if (device == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const facade = DeviceFromInterface(device);
    if (facade->magic != kDeviceMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    re2dj::directx::D3dViewport7 core_viewport;
    core_viewport.x = viewport.x;
    core_viewport.y = viewport.y;
    core_viewport.width = viewport.width;
    core_viewport.height = viewport.height;
    core_viewport.min_z = viewport.min_z;
    core_viewport.max_z = viewport.max_z;
    return static_cast<HRESULT>(re2dj::directx::SetViewport(facade->state, core_viewport));
}

HRESULT LegacyDeviceGetViewport(IDirect3DDevice3* device, LegacyViewportState* viewport)
{
    if (device == nullptr || viewport == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const facade = DeviceFromInterface(device);
    if (facade->magic != kDeviceMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    re2dj::directx::D3dViewport7 core_viewport;
    const auto result = static_cast<HRESULT>(re2dj::directx::GetViewport(facade->state, &core_viewport));
    if (result != D3D_OK)
    {
        return result;
    }
    viewport->x = core_viewport.x;
    viewport->y = core_viewport.y;
    viewport->width = core_viewport.width;
    viewport->height = core_viewport.height;
    viewport->min_z = core_viewport.min_z;
    viewport->max_z = core_viewport.max_z;
    return D3D_OK;
}

HRESULT LegacyDeviceClear(IDirect3DDevice3* device,
                          DWORD rect_count,
                          const D3DRECT* rects,
                          DWORD flags,
                          D3DCOLOR color,
                          float depth,
                          DWORD stencil)
{
    (void)rects;
    (void)stencil;
    if (device == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    DeviceFacade* const facade = DeviceFromInterface(device);
    if (facade->magic != kDeviceMagic)
    {
        return DDERR_INVALIDOBJECT;
    }
    const bool full_target = rect_count == 0;
    const bool clears_target = (flags & D3DCLEAR_TARGET) != 0;
    const bool targets_presentation_surface =
        facade->root != nullptr && facade->render_target != nullptr &&
        facade->render_target == facade->root->presentation_surface;
    if (full_target && clears_target && targets_presentation_surface)
    {
        const std::uint16_t color565 = Rgb565FromD3dColor(color);
        FillSurfaceWithColor(facade->render_target, color565);
        std::string clear_error;
        if (!RequestRenderTargetClear(facade->root, color565, &clear_error))
        {
            re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kOpenGlFailureMessage);
            return DDERR_GENERIC;
        }
    }
    static GraphicsCallLedger ledger = {"Clear", 8};
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&ledger.remaining)) >= 0)
    {
        WriteGraphicsTraceFormat(
            "re2dj:hle:LegacyDeviceClear:rects=%lu:flags=0x%08lx:color=0x%08lx:depth=%.3f",
            static_cast<unsigned long>(rect_count),
            flags,
            static_cast<unsigned long>(color),
            static_cast<double>(depth));
    }
    return D3D_OK;
}

HRESULT LegacyDeviceSetRenderTarget(IDirect3DDevice3* device,
                                    IDirectDrawSurface4* surface,
                                    DWORD flags)
{
    return DeviceSetRenderTarget(device, surface, flags);
}

HRESULT LegacyDeviceGetRenderTarget(IDirect3DDevice3* device, IDirectDrawSurface4** surface)
{
    return DeviceGetRenderTarget(device, surface);
}

HRESULT LegacySurfaceLock(IDirectDrawSurface4* surface,
                          RECT* rect,
                          DDSURFACEDESC2* descriptor,
                          DWORD flags,
                          HANDLE event)
{
    return SurfaceLock(surface, rect, descriptor, flags, event);
}

HRESULT LegacySurfaceUnlock(IDirectDrawSurface4* surface, RECT* rect)
{
    return SurfaceUnlock(surface, rect);
}

}  // namespace re2dj::platform::windows

extern "C" __declspec(dllexport) HRESULT WINAPI Re2djHleDirectDrawCreate(
    GUID*,
    LPDIRECTDRAW* direct_draw,
    IUnknown* outer)
{
    re2dj::platform::windows::WriteRuntimeLog(re2dj::platform::windows::RuntimeLogChannel::kRuntime, kDirectDrawCreateMessage);
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
