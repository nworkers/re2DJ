#define NOMINMAX
#define CINTERFACE
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include <cstdio>
#include <cstring>
#include <new>

#include "directdraw7_com_facade.h"
#include "direct3d7_com_facade.h"
#include "directdraw_com_context.h"
#include "graphics_trace_log.h"

namespace re2dj::platform::windows
{
namespace
{

constexpr GUID kIidDirectDraw = {
    0x6c14db80, 0xa733, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
constexpr GUID kIidDirectDraw2 = {
    0xb10f182e, 0x04a7, 0x11d1, {0xa4, 0x5f, 0x00, 0xaa, 0x00, 0xc7, 0x49, 0x68}};
constexpr GUID kIidDirectDraw4 = {
    0x9c563761, 0x8888, 0x11cf, {0x90, 0xe2, 0x00, 0xaa, 0x00, 0x42, 0x39, 0xe3}};
constexpr GUID kIidDirectDraw7 = {
    0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
constexpr GUID kIidDirect3D7 = {
    0xf5049e77, 0x4861, 0x11d2, {0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8}};

struct DirectDraw7Facade;
struct Surface7Facade;

struct DirectDraw7Facade
{
    IDirectDraw7 interface_value;
    volatile LONG references = 1;
    DirectDrawComContext context;
};

struct Surface7Facade
{
    IDirectDrawSurface7 interface_value;
    volatile LONG references = 1;
    DirectDraw7Facade* root = nullptr;
    DDSURFACEDESC2 desc = {};
};

IDirectDraw7Vtbl* DirectDraw7Vtable();
IDirectDrawSurface7Vtbl* Surface7Vtable();

// Records one ledger line per called vtable method. The count is bounded so a
// per-frame method cannot fill the trace file once rendering starts; the
// question this ledger answers is which methods the guest reaches at all.
void TraceDd7Call(const char* name)
{
    static volatile LONG remaining = 4096;
    if (remaining < 0)
    {
        return;
    }
    if (InterlockedDecrement(&remaining) < 0)
    {
        return;
    }
    WriteGraphicsTraceFormat("re2dj:hle:IDirectDraw7::%s", name);
}

HRESULT WINAPI Dd7QueryInterface(IDirectDraw7* self, REFIID iid, void** object)
{
    char qi_buf[96] = {};
    std::snprintf(qi_buf, sizeof(qi_buf),
                  "re2dj:hle:IDirectDraw7::QueryInterface iid={%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                  iid.Data1, iid.Data2, iid.Data3,
                  iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
                  iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);
    WriteGraphicsTraceLine(qi_buf);

    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;

    auto* const facade = reinterpret_cast<DirectDraw7Facade*>(self);

    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, kIidDirectDraw) ||
        IsEqualGUID(iid, kIidDirectDraw2) || IsEqualGUID(iid, kIidDirectDraw4) ||
        IsEqualGUID(iid, kIidDirectDraw7))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    if (IsEqualGUID(iid, kIidDirect3D7))
    {
        return CreateDirect3D7Facade(&facade->context, object);
    }

    return E_NOINTERFACE;
}

ULONG WINAPI Dd7AddRef(IDirectDraw7* self)
{
    auto* const facade = reinterpret_cast<DirectDraw7Facade*>(self);
    return static_cast<ULONG>(InterlockedIncrement(&facade->references));
}

ULONG WINAPI Dd7Release(IDirectDraw7* self)
{
    auto* const facade = reinterpret_cast<DirectDraw7Facade*>(self);
    const LONG count = InterlockedDecrement(&facade->references);
    if (count <= 0)
    {
        delete facade;
        return 0;
    }
    return static_cast<ULONG>(count);
}

HRESULT WINAPI Dd7Compact(IDirectDraw7*)
{
    TraceDd7Call("Compact");
    return DD_OK;
}
HRESULT WINAPI Dd7CreateClipper(IDirectDraw7*, DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*)
{
    TraceDd7Call("CreateClipper");
    return DD_OK;
}
HRESULT WINAPI Dd7CreatePalette(IDirectDraw7*, DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*)
{
    TraceDd7Call("CreatePalette");
    return DD_OK;
}

HRESULT WINAPI Dd7CreateSurface(IDirectDraw7* self,
                               DDSURFACEDESC2* desc,
                               IDirectDrawSurface7** surface,
                               IUnknown* outer)
{
    (void)outer;
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirectDraw7::CreateSurface flags=0x%08lx caps=0x%08lx %lux%lu",
        static_cast<unsigned long>(desc == nullptr ? 0 : desc->dwFlags),
        static_cast<unsigned long>(desc == nullptr ? 0 : desc->ddsCaps.dwCaps),
        static_cast<unsigned long>(desc == nullptr ? 0 : desc->dwWidth),
        static_cast<unsigned long>(desc == nullptr ? 0 : desc->dwHeight));
    if (surface == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *surface = nullptr;
    auto* const facade = reinterpret_cast<DirectDraw7Facade*>(self);
    auto* const sfacade = new (std::nothrow) Surface7Facade;
    if (sfacade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    sfacade->interface_value.lpVtbl = Surface7Vtable();
    sfacade->root = facade;
    if (desc != nullptr)
    {
        sfacade->desc = *desc;
    }
    *surface = &sfacade->interface_value;
    return DD_OK;
}

HRESULT WINAPI Dd7DuplicateSurface(IDirectDraw7*, IDirectDrawSurface7*, IDirectDrawSurface7**)
{
    TraceDd7Call("DuplicateSurface");
    return DD_OK;
}

// Fills one mode descriptor. The RGB masks follow the standard 5-6-5, 8-8-8,
// and 8-8-8-8 layouts a DirectX 7 driver reports for these depths.
void FillDisplayMode(DDSURFACEDESC2* mode, DWORD width, DWORD height, DWORD depth)
{
    std::memset(mode, 0, sizeof(*mode));
    mode->dwSize = sizeof(*mode);
    mode->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT |
                    DDSD_REFRESHRATE;
    mode->dwWidth = width;
    mode->dwHeight = height;
    mode->dwRefreshRate = 60;
    mode->lPitch = static_cast<LONG>(width * (depth / 8));
    mode->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    mode->ddpfPixelFormat.dwFlags = DDPF_RGB;
    mode->ddpfPixelFormat.dwRGBBitCount = depth;
    switch (depth)
    {
        case 16:
            mode->ddpfPixelFormat.dwRBitMask = 0x0000f800;
            mode->ddpfPixelFormat.dwGBitMask = 0x000007e0;
            mode->ddpfPixelFormat.dwBBitMask = 0x0000001f;
            break;
        case 32:
            mode->ddpfPixelFormat.dwFlags |= DDPF_ALPHAPIXELS;
            mode->ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
            [[fallthrough]];
        case 24:
        default:
            mode->ddpfPixelFormat.dwRBitMask = 0x00ff0000;
            mode->ddpfPixelFormat.dwGBitMask = 0x0000ff00;
            mode->ddpfPixelFormat.dwBBitMask = 0x000000ff;
            break;
    }
}

HRESULT WINAPI Dd7EnumDisplayModes(IDirectDraw7*,
                                  DWORD flags,
                                  LPDDSURFACEDESC2 filter,
                                  LPVOID arg,
                                  LPDDENUMMODESCALLBACK2 callback)
{
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirectDraw7::EnumDisplayModes flags=0x%08lx filter=%s",
        static_cast<unsigned long>(flags),
        filter == nullptr ? "none" : "present");
    if (callback == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    // A single 640x480x16 entry is what the guest saw before, and it left the
    // guest with nothing to select when it wanted another depth or size, so
    // the whole set the legacy backend can present is enumerated.
    struct ModeEntry
    {
        DWORD width;
        DWORD height;
    };
    constexpr ModeEntry kSizes[] = {
        {320, 240}, {512, 384}, {640, 480}, {800, 600}, {1024, 768}};
    constexpr DWORD kDepths[] = {16, 24, 32};

    for (const ModeEntry& size : kSizes)
    {
        for (const DWORD depth : kDepths)
        {
            DDSURFACEDESC2 mode = {};
            FillDisplayMode(&mode, size.width, size.height, depth);
            const HRESULT callback_result = callback(&mode, arg);
            WriteGraphicsTraceFormat(
                "re2dj:hle:IDirectDraw7::EnumDisplayModes:mode %lux%lux%lu refresh=%lu "
                "pitch=%ld callback_ret=0x%08lx",
                static_cast<unsigned long>(mode.dwWidth),
                static_cast<unsigned long>(mode.dwHeight),
                static_cast<unsigned long>(mode.ddpfPixelFormat.dwRGBBitCount),
                static_cast<unsigned long>(mode.dwRefreshRate),
                static_cast<long>(mode.lPitch),
                static_cast<unsigned long>(callback_result));
            if (callback_result == DDENUMRET_CANCEL)
            {
                return DD_OK;
            }
        }
    }
    return DD_OK;
}
HRESULT WINAPI Dd7EnumSurfaces(IDirectDraw7*, DWORD, LPDDSURFACEDESC2, LPVOID, LPDDENUMSURFACESCALLBACK7)
{
    TraceDd7Call("EnumSurfaces");
    return DD_OK;
}
HRESULT WINAPI Dd7FlipToGDISurface(IDirectDraw7*)
{
    TraceDd7Call("FlipToGDISurface");
    return DD_OK;
}

HRESULT WINAPI Dd7GetCaps(IDirectDraw7*, DDCAPS* driver_caps, DDCAPS* hel_caps)
{
    WriteGraphicsTraceLine("re2dj:hle:IDirectDraw7::GetCaps");
    const auto fill = [](DDCAPS* caps) {
        if (caps != nullptr)
        {
            std::memset(caps, 0, sizeof(*caps));
            caps->dwSize = sizeof(*caps);
            caps->dwCaps = DDCAPS_3D | DDCAPS_BLT | DDCAPS_COLORKEY;
            // The guest's driver stage keeps a device only when the driver
            // reports DDCAPS2_CANRENDERWINDOWED, so the facade must publish it.
            // The rest are properties this facade genuinely has: it vouches for
            // its own behavior, its surfaces live in host memory with no page
            // lock, and it does not reject surfaces wider than the display.
            caps->dwCaps2 = DDCAPS2_CERTIFIED | DDCAPS2_NOPAGELOCKREQUIRED |
                            DDCAPS2_WIDESURFACES | DDCAPS2_CANRENDERWINDOWED;
            caps->ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_BACKBUFFER |
                                   DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
        }
    };
    fill(driver_caps);
    fill(hel_caps);
    return DD_OK;
}

HRESULT WINAPI Dd7GetDisplayMode(IDirectDraw7*, LPDDSURFACEDESC2 desc)
{
    TraceDd7Call("GetDisplayMode");
    if (desc != nullptr)
    {
        std::memset(desc, 0, sizeof(*desc));
        desc->dwSize = sizeof(*desc);
        desc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
        desc->dwWidth = 640;
        desc->dwHeight = 480;
        desc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        desc->ddpfPixelFormat.dwFlags = DDPF_RGB;
        desc->ddpfPixelFormat.dwRGBBitCount = 16;
    }
    return DD_OK;
}

HRESULT WINAPI Dd7GetFourCCCodes(IDirectDraw7*, LPDWORD, LPDWORD)
{
    TraceDd7Call("GetFourCCCodes");
    return DD_OK;
}
HRESULT WINAPI Dd7GetGDISurface(IDirectDraw7*, IDirectDrawSurface7**)
{
    TraceDd7Call("GetGDISurface");
    return DD_OK;
}
HRESULT WINAPI Dd7GetMonitorFrequency(IDirectDraw7*, LPDWORD freq)
{
    TraceDd7Call("GetMonitorFrequency");
    if (freq != nullptr) *freq = 60;
    return DD_OK;
}
HRESULT WINAPI Dd7GetScanLine(IDirectDraw7*, LPDWORD line)
{
    TraceDd7Call("GetScanLine");
    if (line != nullptr) *line = 0;
    return DD_OK;
}
HRESULT WINAPI Dd7GetVerticalBlankStatus(IDirectDraw7*, LPBOOL status)
{
    TraceDd7Call("GetVerticalBlankStatus");
    if (status != nullptr) *status = TRUE;
    return DD_OK;
}
HRESULT WINAPI Dd7Initialize(IDirectDraw7*, GUID*)
{
    TraceDd7Call("Initialize");
    return DD_OK;
}
HRESULT WINAPI Dd7RestoreDisplayMode(IDirectDraw7*)
{
    TraceDd7Call("RestoreDisplayMode");
    return DD_OK;
}

HRESULT WINAPI Dd7SetCooperativeLevel(IDirectDraw7* self, HWND window, DWORD flags)
{
    char coop_buf[80] = {};
    std::snprintf(coop_buf, sizeof(coop_buf),
                  "re2dj:hle:IDirectDraw7::SetCooperativeLevel hwnd=0x%08x flags=0x%08x",
                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(window)),
                  static_cast<unsigned>(flags));
    WriteGraphicsTraceLine(coop_buf);

    auto* const facade = reinterpret_cast<DirectDraw7Facade*>(self);
    facade->context.window = window;
    return DD_OK;
}

HRESULT WINAPI Dd7SetDisplayMode(IDirectDraw7* self,
                                DWORD width,
                                DWORD height,
                                DWORD bpp,
                                DWORD refresh_rate,
                                DWORD flags)
{
    (void)refresh_rate;
    (void)flags;
    char mode_buf[80] = {};
    std::snprintf(mode_buf, sizeof(mode_buf),
                  "re2dj:hle:IDirectDraw7::SetDisplayMode %ux%ux%u",
                  static_cast<unsigned>(width),
                  static_cast<unsigned>(height),
                  static_cast<unsigned>(bpp));
    WriteGraphicsTraceLine(mode_buf);

    auto* const facade = reinterpret_cast<DirectDraw7Facade*>(self);
    facade->context.width = width;
    facade->context.height = height;
    facade->context.bits_per_pixel = bpp;
    return DD_OK;
}

HRESULT WINAPI Dd7WaitForVerticalBlank(IDirectDraw7*, DWORD, HANDLE)
{
    TraceDd7Call("WaitForVerticalBlank");
    return DD_OK;
}
HRESULT WINAPI Dd7GetAvailableVidMem(IDirectDraw7*, LPDDSCAPS2, LPDWORD total, LPDWORD free)
{
    TraceDd7Call("GetAvailableVidMem");
    if (total != nullptr) *total = 64 * 1024 * 1024;
    if (free != nullptr) *free = 64 * 1024 * 1024;
    return DD_OK;
}
HRESULT WINAPI Dd7GetSurfaceFromDC(IDirectDraw7*, HDC, IDirectDrawSurface7**)
{
    TraceDd7Call("GetSurfaceFromDC");
    return DD_OK;
}
HRESULT WINAPI Dd7RestoreAllSurfaces(IDirectDraw7*)
{
    TraceDd7Call("RestoreAllSurfaces");
    return DD_OK;
}
HRESULT WINAPI Dd7TestCooperativeLevel(IDirectDraw7*)
{
    TraceDd7Call("TestCooperativeLevel");
    return DD_OK;
}
HRESULT WINAPI Dd7GetDeviceIdentifier(IDirectDraw7*, LPDDDEVICEIDENTIFIER2, DWORD)
{
    TraceDd7Call("GetDeviceIdentifier");
    return DD_OK;
}
HRESULT WINAPI Dd7StartModeTest(IDirectDraw7*, LPSIZE, DWORD, DWORD)
{
    TraceDd7Call("StartModeTest");
    return DD_OK;
}
HRESULT WINAPI Dd7EvaluateMode(IDirectDraw7*, DWORD, DWORD*)
{
    TraceDd7Call("EvaluateMode");
    return DD_OK;
}

IDirectDraw7Vtbl* DirectDraw7Vtable()
{
    static IDirectDraw7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = Dd7QueryInterface;
        table.AddRef = Dd7AddRef;
        table.Release = Dd7Release;
        table.Compact = Dd7Compact;
        table.CreateClipper = Dd7CreateClipper;
        table.CreatePalette = Dd7CreatePalette;
        table.CreateSurface = Dd7CreateSurface;
        table.DuplicateSurface = Dd7DuplicateSurface;
        table.EnumDisplayModes = Dd7EnumDisplayModes;
        table.EnumSurfaces = Dd7EnumSurfaces;
        table.FlipToGDISurface = Dd7FlipToGDISurface;
        table.GetCaps = Dd7GetCaps;
        table.GetDisplayMode = Dd7GetDisplayMode;
        table.GetFourCCCodes = Dd7GetFourCCCodes;
        table.GetGDISurface = Dd7GetGDISurface;
        table.GetMonitorFrequency = Dd7GetMonitorFrequency;
        table.GetScanLine = Dd7GetScanLine;
        table.GetVerticalBlankStatus = Dd7GetVerticalBlankStatus;
        table.Initialize = Dd7Initialize;
        table.RestoreDisplayMode = Dd7RestoreDisplayMode;
        table.SetCooperativeLevel = Dd7SetCooperativeLevel;
        table.SetDisplayMode = Dd7SetDisplayMode;
        table.WaitForVerticalBlank = Dd7WaitForVerticalBlank;
        table.GetAvailableVidMem = Dd7GetAvailableVidMem;
        table.GetSurfaceFromDC = Dd7GetSurfaceFromDC;
        table.RestoreAllSurfaces = Dd7RestoreAllSurfaces;
        table.TestCooperativeLevel = Dd7TestCooperativeLevel;
        table.GetDeviceIdentifier = Dd7GetDeviceIdentifier;
        table.StartModeTest = Dd7StartModeTest;
        table.EvaluateMode = Dd7EvaluateMode;
        initialized = true;
    }
    return &table;
}

// Surface7 methods
HRESULT WINAPI Surf7QueryInterface(IDirectDrawSurface7* self, REFIID iid, void** object)
{
    if (object == nullptr) return E_POINTER;
    *object = nullptr;
    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IDirectDrawSurface7))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
ULONG WINAPI Surf7AddRef(IDirectDrawSurface7* self)
{
    auto* const s = reinterpret_cast<Surface7Facade*>(self);
    return static_cast<ULONG>(InterlockedIncrement(&s->references));
}
ULONG WINAPI Surf7Release(IDirectDrawSurface7* self)
{
    auto* const s = reinterpret_cast<Surface7Facade*>(self);
    const LONG count = InterlockedDecrement(&s->references);
    if (count <= 0)
    {
        delete s;
        return 0;
    }
    return static_cast<ULONG>(count);
}

HRESULT WINAPI Surf7AddAttachedSurface(IDirectDrawSurface7*, IDirectDrawSurface7*) { return DD_OK; }
HRESULT WINAPI Surf7AddOverlayDirtyRect(IDirectDrawSurface7*, LPRECT) { return DD_OK; }
HRESULT WINAPI Surf7Blt(IDirectDrawSurface7*, LPRECT, IDirectDrawSurface7*, LPRECT, DWORD, LPDDBLTFX) { return DD_OK; }
HRESULT WINAPI Surf7BltBatch(IDirectDrawSurface7*, LPDDBLTBATCH, DWORD, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7BltFast(IDirectDrawSurface7*, DWORD, DWORD, IDirectDrawSurface7*, LPRECT, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7DeleteAttachedSurface(IDirectDrawSurface7*, DWORD, IDirectDrawSurface7*) { return DD_OK; }
HRESULT WINAPI Surf7EnumAttachedSurfaces(IDirectDrawSurface7*, LPVOID, LPDDENUMSURFACESCALLBACK7) { return DD_OK; }
HRESULT WINAPI Surf7EnumOverlayZOrders(IDirectDrawSurface7*, DWORD, LPVOID, LPDDENUMSURFACESCALLBACK7) { return DD_OK; }
HRESULT WINAPI Surf7Flip(IDirectDrawSurface7*, IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7GetAttachedSurface(IDirectDrawSurface7* self, LPDDSCAPS2, IDirectDrawSurface7** att)
{
    if (att == nullptr) return DDERR_INVALIDPARAMS;
    *att = self;
    self->lpVtbl->AddRef(self);
    return DD_OK;
}
HRESULT WINAPI Surf7GetBltStatus(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7GetCaps(IDirectDrawSurface7* self, LPDDSCAPS2 caps)
{
    if (caps != nullptr)
    {
        auto* const s = reinterpret_cast<Surface7Facade*>(self);
        caps->dwCaps = s->desc.ddsCaps.dwCaps;
    }
    return DD_OK;
}
HRESULT WINAPI Surf7GetClipper(IDirectDrawSurface7*, LPDIRECTDRAWCLIPPER*) { return DD_OK; }
HRESULT WINAPI Surf7GetColorKey(IDirectDrawSurface7*, DWORD, LPDDCOLORKEY) { return DD_OK; }
HRESULT WINAPI Surf7GetDC(IDirectDrawSurface7*, HDC* hdc)
{
    if (hdc != nullptr) *hdc = nullptr;
    return DD_OK;
}
HRESULT WINAPI Surf7GetFlipStatus(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7GetOverlayPosition(IDirectDrawSurface7*, LPLONG, LPLONG) { return DD_OK; }
HRESULT WINAPI Surf7GetPalette(IDirectDrawSurface7*, LPDIRECTDRAWPALETTE*) { return DD_OK; }
HRESULT WINAPI Surf7GetPixelFormat(IDirectDrawSurface7*, LPDDPIXELFORMAT pf)
{
    if (pf != nullptr)
    {
        std::memset(pf, 0, sizeof(*pf));
        pf->dwSize = sizeof(*pf);
        pf->dwFlags = DDPF_RGB;
        pf->dwRGBBitCount = 16;
        pf->dwRBitMask = 0xf800;
        pf->dwGBitMask = 0x07e0;
        pf->dwBBitMask = 0x001f;
    }
    return DD_OK;
}
HRESULT WINAPI Surf7GetSurfaceDesc(IDirectDrawSurface7* self, DDSURFACEDESC2* desc)
{
    if (desc != nullptr)
    {
        auto* const s = reinterpret_cast<Surface7Facade*>(self);
        *desc = s->desc;
    }
    return DD_OK;
}
HRESULT WINAPI Surf7Initialize(IDirectDrawSurface7*, IDirectDraw*, DDSURFACEDESC2*) { return DD_OK; }
HRESULT WINAPI Surf7IsLost(IDirectDrawSurface7*) { return DD_OK; }
HRESULT WINAPI Surf7Lock(IDirectDrawSurface7*, LPRECT, DDSURFACEDESC2*, DWORD, HANDLE) { return DD_OK; }
HRESULT WINAPI Surf7ReleaseDC(IDirectDrawSurface7*, HDC) { return DD_OK; }
HRESULT WINAPI Surf7Restore(IDirectDrawSurface7*) { return DD_OK; }
HRESULT WINAPI Surf7SetClipper(IDirectDrawSurface7*, LPDIRECTDRAWCLIPPER) { return DD_OK; }
HRESULT WINAPI Surf7SetColorKey(IDirectDrawSurface7*, DWORD, LPDDCOLORKEY) { return DD_OK; }
HRESULT WINAPI Surf7SetOverlayPosition(IDirectDrawSurface7*, LONG, LONG) { return DD_OK; }
HRESULT WINAPI Surf7SetPalette(IDirectDrawSurface7*, LPDIRECTDRAWPALETTE) { return DD_OK; }
HRESULT WINAPI Surf7Unlock(IDirectDrawSurface7*, LPRECT) { return DD_OK; }
HRESULT WINAPI Surf7UpdateOverlay(IDirectDrawSurface7*, LPRECT, IDirectDrawSurface7*, LPRECT, DWORD, LPDDOVERLAYFX) { return DD_OK; }
HRESULT WINAPI Surf7UpdateOverlayDisplay(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7UpdateOverlayZOrder(IDirectDrawSurface7*, DWORD, IDirectDrawSurface7*) { return DD_OK; }
HRESULT WINAPI Surf7GetDDInterface(IDirectDrawSurface7* self, void** ddi)
{
    if (ddi == nullptr) return DDERR_INVALIDPARAMS;
    auto* const s = reinterpret_cast<Surface7Facade*>(self);
    *ddi = s->root;
    s->root->interface_value.lpVtbl->AddRef(&s->root->interface_value);
    return DD_OK;
}
HRESULT WINAPI Surf7PageLock(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7PageUnlock(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7SetSurfaceDesc(IDirectDrawSurface7*, DDSURFACEDESC2*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7SetPrivateData(IDirectDrawSurface7*, REFGUID, void*, DWORD, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7GetPrivateData(IDirectDrawSurface7*, REFGUID, void*, DWORD*) { return DD_OK; }
HRESULT WINAPI Surf7FreePrivateData(IDirectDrawSurface7*, REFGUID) { return DD_OK; }
HRESULT WINAPI Surf7GetUniquenessValue(IDirectDrawSurface7*, LPDWORD val) { if (val) *val = 0; return DD_OK; }
HRESULT WINAPI Surf7ChangeUniquenessValue(IDirectDrawSurface7*) { return DD_OK; }
HRESULT WINAPI Surf7SetPriority(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7GetPriority(IDirectDrawSurface7*, LPDWORD val) { if (val) *val = 0; return DD_OK; }
HRESULT WINAPI Surf7SetLOD(IDirectDrawSurface7*, DWORD) { return DD_OK; }
HRESULT WINAPI Surf7GetLOD(IDirectDrawSurface7*, LPDWORD val) { if (val) *val = 0; return DD_OK; }

IDirectDrawSurface7Vtbl* Surface7Vtable()
{
    static IDirectDrawSurface7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        table.QueryInterface = Surf7QueryInterface;
        table.AddRef = Surf7AddRef;
        table.Release = Surf7Release;
        table.AddAttachedSurface = Surf7AddAttachedSurface;
        table.AddOverlayDirtyRect = Surf7AddOverlayDirtyRect;
        table.Blt = Surf7Blt;
        table.BltBatch = Surf7BltBatch;
        table.BltFast = Surf7BltFast;
        table.DeleteAttachedSurface = Surf7DeleteAttachedSurface;
        table.EnumAttachedSurfaces = Surf7EnumAttachedSurfaces;
        table.EnumOverlayZOrders = Surf7EnumOverlayZOrders;
        table.Flip = Surf7Flip;
        table.GetAttachedSurface = Surf7GetAttachedSurface;
        table.GetBltStatus = Surf7GetBltStatus;
        table.GetCaps = Surf7GetCaps;
        table.GetClipper = Surf7GetClipper;
        table.GetColorKey = Surf7GetColorKey;
        table.GetDC = Surf7GetDC;
        table.GetFlipStatus = Surf7GetFlipStatus;
        table.GetOverlayPosition = Surf7GetOverlayPosition;
        table.GetPalette = Surf7GetPalette;
        table.GetPixelFormat = Surf7GetPixelFormat;
        table.GetSurfaceDesc = Surf7GetSurfaceDesc;
        table.Initialize = Surf7Initialize;
        table.IsLost = Surf7IsLost;
        table.Lock = Surf7Lock;
        table.ReleaseDC = Surf7ReleaseDC;
        table.Restore = Surf7Restore;
        table.SetClipper = Surf7SetClipper;
        table.SetColorKey = Surf7SetColorKey;
        table.SetOverlayPosition = Surf7SetOverlayPosition;
        table.SetPalette = Surf7SetPalette;
        table.Unlock = Surf7Unlock;
        table.UpdateOverlay = Surf7UpdateOverlay;
        table.UpdateOverlayDisplay = Surf7UpdateOverlayDisplay;
        table.UpdateOverlayZOrder = Surf7UpdateOverlayZOrder;
        table.GetDDInterface = Surf7GetDDInterface;
        table.PageLock = Surf7PageLock;
        table.PageUnlock = Surf7PageUnlock;
        table.SetSurfaceDesc = Surf7SetSurfaceDesc;
        table.SetPrivateData = Surf7SetPrivateData;
        table.GetPrivateData = Surf7GetPrivateData;
        table.FreePrivateData = Surf7FreePrivateData;
        table.GetUniquenessValue = Surf7GetUniquenessValue;
        table.ChangeUniquenessValue = Surf7ChangeUniquenessValue;
        table.SetPriority = Surf7SetPriority;
        table.GetPriority = Surf7GetPriority;
        table.SetLOD = Surf7SetLOD;
        table.GetLOD = Surf7GetLOD;
        initialized = true;
    }
    return &table;
}

}  // namespace
}  // namespace re2dj::platform::windows

extern "C" __declspec(dllexport) HRESULT WINAPI
Re2djHleDirectDrawCreateEx(GUID* driver_guid,
                          void** direct_draw,
                          REFIID iid,
                          IUnknown* outer)
{
    (void)outer;

    // The driver GUID decides one half of the guest's device gate: it keeps a
    // driver's devices only when the primary display driver is enumerated,
    // which DirectDraw signals with a null GUID. Driver enumeration itself is
    // still served by the host, so this is where that value becomes visible.
    char driver_text[48] = "null";
    if (driver_guid != nullptr)
    {
        std::snprintf(driver_text, sizeof(driver_text),
                      "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                      driver_guid->Data1, driver_guid->Data2, driver_guid->Data3,
                      driver_guid->Data4[0], driver_guid->Data4[1],
                      driver_guid->Data4[2], driver_guid->Data4[3],
                      driver_guid->Data4[4], driver_guid->Data4[5],
                      driver_guid->Data4[6], driver_guid->Data4[7]);
    }

    char msg_buf[160] = {};
    std::snprintf(msg_buf, sizeof(msg_buf),
                  "re2dj:hle:DirectDrawCreateEx driver=%s iid={%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                  driver_text,
                  iid.Data1, iid.Data2, iid.Data3,
                  iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
                  iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);
    re2dj::platform::windows::WriteGraphicsTraceLine(msg_buf);

    if (direct_draw == nullptr)
    {
        return E_POINTER;
    }
    *direct_draw = nullptr;

    auto* const facade = new (std::nothrow) re2dj::platform::windows::DirectDraw7Facade;
    if (facade == nullptr)
    {
        return DDERR_OUTOFMEMORY;
    }
    facade->interface_value.lpVtbl = re2dj::platform::windows::DirectDraw7Vtable();

    const HRESULT hr = facade->interface_value.lpVtbl->QueryInterface(
        &facade->interface_value, iid, direct_draw);
    facade->interface_value.lpVtbl->Release(&facade->interface_value);
    return hr;
}
