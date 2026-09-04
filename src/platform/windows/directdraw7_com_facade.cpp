#define NOMINMAX
#define CINTERFACE
#include <windows.h>
#include <ddraw.h>

#include <cstdio>
#include <cstring>

#include "directdraw7_com_facade.h"
#include "direct3d7_com_facade.h"
#include "direct3d7_vertex_buffer_facade.h"
#include "directdraw_legacy_interop.h"
#include "graphics_trace_log.h"

// IDirectDraw7 and IDirectDrawSurface7 repeat their version 4 predecessors slot
// for slot and append to them, so this file is mostly a table of adoptions: a
// slot whose behavior did not change between the versions holds the DirectX 6
// function itself, and the object behind the interface is the same object that
// file creates. Only DirectX 7's own slots, and the ones whose parameters
// changed, get code here.
//
// See directdraw_legacy_interop.h for the layering this rests on.

namespace re2dj::platform::windows
{
namespace
{

// Only the identifiers DirectX 7 added, plus the version 1 surface identifier
// the DirectX 6 implementation does not answer. Every other identifier is
// forwarded to that implementation rather than repeated here.
constexpr GUID kIidDirectDraw7 = {
    0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
constexpr GUID kIidDirect3D7 = {
    0xf5049e77, 0x4861, 0x11d2, {0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8}};
constexpr GUID kIidDirectDrawSurface = {
    0x6c14db81, 0xa733, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
constexpr GUID kIidDirectDrawSurface7 = {
    0x06675a80, 0x3b9b, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};

// The number of calls one unimplemented slot records before going quiet.
constexpr long kUnimplementedCallBudget = 4;

// Installs a DirectX 6 implementation into a DirectX 7 vtable slot. The two
// declarations differ only in the static types of pointer parameters, which are
// the same width and are passed the same way, so one function serves both
// interfaces. Assigning slot by slot rather than copying the table wholesale
// keeps the compiler checking that both members exist.
template <typename Slot, typename Implementation>
void Adopt(Slot& slot, Implementation implementation)
{
    slot = reinterpret_cast<Slot>(implementation);
}

// The DirectX 6 root reached through its own interface type. The object behind
// an IDirectDraw7 this facade hands out is the DirectX 6 root, so the cast is
// the same pointer.
IDirectDraw4* LegacyRoot(IDirectDraw7* self)
{
    return reinterpret_cast<IDirectDraw4*>(self);
}

IDirectDrawSurface4* LegacySurface(IDirectDrawSurface7* self)
{
    return reinterpret_cast<IDirectDrawSurface4*>(self);
}

// ---------------------------------------------------------------------------
// IDirectDrawSurface7: slots DirectX 7 changed or added
// ---------------------------------------------------------------------------

// DirectX 7 added its own surface identifier; every other interface a surface
// can be asked for is the one the DirectX 6 implementation already answers,
// including the Direct3D texture interface that lives on the same object.
HRESULT WINAPI Surf7QueryInterface(IDirectDrawSurface7* self, REFIID iid, void** object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    HRESULT result = S_OK;
    if (IsEqualGUID(iid, kIidDirectDrawSurface7) ||
        IsEqualGUID(iid, kIidDirectDrawSurface))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
    }
    else
    {
        result = LegacyDirectDrawSurfaceVtable()->QueryInterface(
            LegacySurface(self), iid, object);
    }
    WriteGraphicsTraceFormat(
        "re2dj:hle:IDirectDrawSurface7::QueryInterface "
        "iid={%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} result=0x%08lx",
        iid.Data1, iid.Data2, iid.Data3,
        iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
        iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7],
        static_cast<unsigned long>(result));
    return result;
}

// DirectX 7 added texture management the render backend does not model: there
// is no managed pool to prioritize and no mip chain to pick a level from. The
// values are stored so a read-back matches the write.
HRESULT WINAPI Surf7SetPriority(IDirectDrawSurface7*, DWORD)
{
    return DD_OK;
}
HRESULT WINAPI Surf7GetPriority(IDirectDrawSurface7*, LPDWORD value)
{
    if (value == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *value = 0;
    return DD_OK;
}
HRESULT WINAPI Surf7SetLOD(IDirectDrawSurface7*, DWORD)
{
    return DD_OK;
}
HRESULT WINAPI Surf7GetLOD(IDirectDrawSurface7*, LPDWORD value)
{
    if (value == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *value = 0;
    return DD_OK;
}

// ---------------------------------------------------------------------------
// IDirectDrawSurface7: slots with no implementation yet
// ---------------------------------------------------------------------------

#define RE2DJ_SURFACE7_UNIMPLEMENTED(name)                                   \
    do                                                                       \
    {                                                                        \
        static GraphicsCallLedger ledger = {name, kUnimplementedCallBudget};  \
        ReportUnimplementedGraphicsCall("IDirectDrawSurface7", &ledger);      \
    } while (false)

HRESULT WINAPI Surf7AddOverlayDirtyRect(IDirectDrawSurface7*, LPRECT)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("AddOverlayDirtyRect");
    return DD_OK;
}
HRESULT WINAPI Surf7BltBatch(IDirectDrawSurface7*, LPDDBLTBATCH, DWORD, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("BltBatch");
    return DD_OK;
}
HRESULT WINAPI Surf7DeleteAttachedSurface(IDirectDrawSurface7*, DWORD, IDirectDrawSurface7*)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("DeleteAttachedSurface");
    return DD_OK;
}
HRESULT WINAPI Surf7EnumAttachedSurfaces(IDirectDrawSurface7*, LPVOID, LPDDENUMSURFACESCALLBACK7)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("EnumAttachedSurfaces");
    return DD_OK;
}
HRESULT WINAPI Surf7EnumOverlayZOrders(IDirectDrawSurface7*, DWORD, LPVOID, LPDDENUMSURFACESCALLBACK7)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("EnumOverlayZOrders");
    return DD_OK;
}
HRESULT WINAPI Surf7GetBltStatus(IDirectDrawSurface7*, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetBltStatus");
    return DD_OK;
}
HRESULT WINAPI Surf7GetClipper(IDirectDrawSurface7*, LPDIRECTDRAWCLIPPER* clipper)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetClipper");
    if (clipper != nullptr)
    {
        *clipper = nullptr;
    }
    return DDERR_NOCLIPPERATTACHED;
}
HRESULT WINAPI Surf7GetColorKey(IDirectDrawSurface7*, DWORD, LPDDCOLORKEY)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetColorKey");
    return DDERR_NOCOLORKEY;
}
HRESULT WINAPI Surf7GetFlipStatus(IDirectDrawSurface7*, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetFlipStatus");
    return DD_OK;
}
HRESULT WINAPI Surf7GetOverlayPosition(IDirectDrawSurface7*, LPLONG, LPLONG)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetOverlayPosition");
    return DDERR_NOTAOVERLAYSURFACE;
}
HRESULT WINAPI Surf7GetPalette(IDirectDrawSurface7*, LPDIRECTDRAWPALETTE* palette)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetPalette");
    if (palette != nullptr)
    {
        *palette = nullptr;
    }
    return DDERR_NOPALETTEATTACHED;
}
HRESULT WINAPI Surf7Initialize(IDirectDrawSurface7*, IDirectDraw*, DDSURFACEDESC2*)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("Initialize");
    return DDERR_ALREADYINITIALIZED;
}
HRESULT WINAPI Surf7SetClipper(IDirectDrawSurface7*, LPDIRECTDRAWCLIPPER)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("SetClipper");
    return DD_OK;
}
HRESULT WINAPI Surf7SetOverlayPosition(IDirectDrawSurface7*, LONG, LONG)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("SetOverlayPosition");
    return DDERR_NOTAOVERLAYSURFACE;
}
HRESULT WINAPI Surf7SetPalette(IDirectDrawSurface7*, LPDIRECTDRAWPALETTE)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("SetPalette");
    return DD_OK;
}
HRESULT WINAPI Surf7UpdateOverlay(IDirectDrawSurface7*, LPRECT, IDirectDrawSurface7*, LPRECT, DWORD, LPDDOVERLAYFX)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("UpdateOverlay");
    return DDERR_NOTAOVERLAYSURFACE;
}
HRESULT WINAPI Surf7UpdateOverlayDisplay(IDirectDrawSurface7*, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("UpdateOverlayDisplay");
    return DDERR_NOTAOVERLAYSURFACE;
}
HRESULT WINAPI Surf7UpdateOverlayZOrder(IDirectDrawSurface7*, DWORD, IDirectDrawSurface7*)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("UpdateOverlayZOrder");
    return DDERR_NOTAOVERLAYSURFACE;
}
HRESULT WINAPI Surf7GetDDInterface(IDirectDrawSurface7*, void** interface_value)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetDDInterface");
    if (interface_value != nullptr)
    {
        *interface_value = nullptr;
    }
    return DDERR_INVALIDOBJECT;
}
HRESULT WINAPI Surf7PageLock(IDirectDrawSurface7*, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("PageLock");
    return DD_OK;
}
HRESULT WINAPI Surf7PageUnlock(IDirectDrawSurface7*, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("PageUnlock");
    return DD_OK;
}
HRESULT WINAPI Surf7SetSurfaceDesc(IDirectDrawSurface7*, DDSURFACEDESC2*, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("SetSurfaceDesc");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Surf7SetPrivateData(IDirectDrawSurface7*, REFGUID, void*, DWORD, DWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("SetPrivateData");
    return DD_OK;
}
HRESULT WINAPI Surf7GetPrivateData(IDirectDrawSurface7*, REFGUID, void*, LPDWORD)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetPrivateData");
    return DDERR_NOTFOUND;
}
HRESULT WINAPI Surf7FreePrivateData(IDirectDrawSurface7*, REFGUID)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("FreePrivateData");
    return DDERR_NOTFOUND;
}
HRESULT WINAPI Surf7GetUniquenessValue(IDirectDrawSurface7*, LPDWORD value)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("GetUniquenessValue");
    if (value != nullptr)
    {
        *value = 0;
    }
    return DD_OK;
}
HRESULT WINAPI Surf7ChangeUniquenessValue(IDirectDrawSurface7*)
{
    RE2DJ_SURFACE7_UNIMPLEMENTED("ChangeUniquenessValue");
    return DD_OK;
}

#undef RE2DJ_SURFACE7_UNIMPLEMENTED

IDirectDrawSurface7Vtbl* Surface7Vtable()
{
    static IDirectDrawSurface7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        const IDirectDrawSurface4Vtbl* const legacy = LegacyDirectDrawSurfaceVtable();

        // Adopted: the DirectX 6 implementation, unchanged.
        Adopt(table.AddRef, legacy->AddRef);
        Adopt(table.Release, legacy->Release);
        Adopt(table.AddAttachedSurface, legacy->AddAttachedSurface);
        Adopt(table.Blt, legacy->Blt);
        Adopt(table.BltFast, legacy->BltFast);
        Adopt(table.Flip, legacy->Flip);
        Adopt(table.GetAttachedSurface, legacy->GetAttachedSurface);
        Adopt(table.GetCaps, legacy->GetCaps);
        Adopt(table.GetDC, legacy->GetDC);
        Adopt(table.GetPixelFormat, legacy->GetPixelFormat);
        Adopt(table.GetSurfaceDesc, legacy->GetSurfaceDesc);
        Adopt(table.IsLost, legacy->IsLost);
        Adopt(table.Lock, legacy->Lock);
        Adopt(table.ReleaseDC, legacy->ReleaseDC);
        Adopt(table.Restore, legacy->Restore);
        Adopt(table.SetColorKey, legacy->SetColorKey);
        Adopt(table.Unlock, legacy->Unlock);

        // DirectX 7's own slots and the ones it changed.
        table.QueryInterface = Surf7QueryInterface;
        table.SetPriority = Surf7SetPriority;
        table.GetPriority = Surf7GetPriority;
        table.SetLOD = Surf7SetLOD;
        table.GetLOD = Surf7GetLOD;

        // No implementation yet; each records the calls it receives.
        table.AddOverlayDirtyRect = Surf7AddOverlayDirtyRect;
        table.BltBatch = Surf7BltBatch;
        table.DeleteAttachedSurface = Surf7DeleteAttachedSurface;
        table.EnumAttachedSurfaces = Surf7EnumAttachedSurfaces;
        table.EnumOverlayZOrders = Surf7EnumOverlayZOrders;
        table.GetBltStatus = Surf7GetBltStatus;
        table.GetClipper = Surf7GetClipper;
        table.GetColorKey = Surf7GetColorKey;
        table.GetFlipStatus = Surf7GetFlipStatus;
        table.GetOverlayPosition = Surf7GetOverlayPosition;
        table.GetPalette = Surf7GetPalette;
        table.Initialize = Surf7Initialize;
        table.SetClipper = Surf7SetClipper;
        table.SetOverlayPosition = Surf7SetOverlayPosition;
        table.SetPalette = Surf7SetPalette;
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
        initialized = true;
    }
    return &table;
}

// ---------------------------------------------------------------------------
// IDirectDraw7: slots DirectX 7 changed or added
// ---------------------------------------------------------------------------

HRESULT WINAPI Dd7QueryInterface(IDirectDraw7* self, REFIID iid, void** object)
{
    char qi_buf[112] = {};
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
    if (IsEqualGUID(iid, kIidDirectDraw7))
    {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    if (IsEqualGUID(iid, kIidDirect3D7))
    {
        return CreateDirect3D7Facade(LegacyRoot(self), object);
    }
    // Every other identifier a root answers - the earlier DirectDraw versions
    // and IDirect3D3 - is one the DirectX 6 implementation already knows, and
    // the pointer it returns is this same object.
    return LegacyDirectDrawVtable()->QueryInterface(LegacyRoot(self), iid, object);
}

// A DirectX 7 driver reports capabilities the DirectX 6 facade does not, and
// the 4th guest's driver stage keeps a device only when the driver publishes
// DDCAPS2_CANRENDERWINDOWED. The rest are properties this facade genuinely has:
// it vouches for its own behavior, its surfaces live in host memory with no
// page lock, and it does not reject surfaces wider than the display.
HRESULT WINAPI Dd7GetCaps(IDirectDraw7*, DDCAPS* driver_caps, DDCAPS* hel_caps)
{
    WriteGraphicsTraceLine("re2dj:hle:IDirectDraw7::GetCaps");
    const auto fill = [](DDCAPS* caps) {
        if (caps != nullptr)
        {
            std::memset(caps, 0, sizeof(*caps));
            caps->dwSize = sizeof(*caps);
            caps->dwCaps = DDCAPS_3D | DDCAPS_BLT | DDCAPS_COLORKEY;
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
            if (callback(&mode, arg) == DDENUMRET_CANCEL)
            {
                return DD_OK;
            }
        }
    }
    return DD_OK;
}

HRESULT WINAPI Dd7GetDisplayMode(IDirectDraw7*, LPDDSURFACEDESC2 desc)
{
    if (desc == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    FillDisplayMode(desc, 640, 480, 16);
    return DD_OK;
}

HRESULT WINAPI Dd7GetMonitorFrequency(IDirectDraw7*, LPDWORD frequency)
{
    if (frequency == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    *frequency = 60;
    return DD_OK;
}

// The guest sweeps its surfaces before restoring them. This facade never loses
// a surface, so there is nothing to enumerate and the sweep finds nothing.
HRESULT WINAPI Dd7EnumSurfaces(IDirectDraw7*, DWORD, LPDDSURFACEDESC2, LPVOID, LPDDENUMSURFACESCALLBACK7)
{
    return DD_OK;
}

HRESULT WINAPI Dd7TestCooperativeLevel(IDirectDraw7*)
{
    return DD_OK;
}

HRESULT WINAPI Dd7WaitForVerticalBlank(IDirectDraw7*, DWORD, HANDLE)
{
    // The backend presents on the guest's flip and paces itself there, so this
    // returns without blocking rather than adding a second wait.
    return DD_OK;
}

HRESULT WINAPI Dd7GetAvailableVidMem(IDirectDraw7*, LPDDSCAPS2, LPDWORD total, LPDWORD free)
{
    // Surfaces live in host memory, so the guest is told it has a fixed budget
    // large enough not to gate its allocations.
    constexpr DWORD kReportedBytes = 128u * 1024u * 1024u;
    if (total != nullptr)
    {
        *total = kReportedBytes;
    }
    if (free != nullptr)
    {
        *free = kReportedBytes;
    }
    return DD_OK;
}

HRESULT WINAPI Dd7GetDeviceIdentifier(IDirectDraw7*, LPDDDEVICEIDENTIFIER2 identifier, DWORD)
{
    if (identifier == nullptr)
    {
        return DDERR_INVALIDPARAMS;
    }
    std::memset(identifier, 0, sizeof(*identifier));
    std::strncpy(identifier->szDriver, "re2dj.dll", sizeof(identifier->szDriver) - 1);
    std::strncpy(identifier->szDescription,
                 "re2DJ HLE Direct3D 7",
                 sizeof(identifier->szDescription) - 1);
    return DD_OK;
}

// ---------------------------------------------------------------------------
// IDirectDraw7: slots with no implementation yet
// ---------------------------------------------------------------------------

#define RE2DJ_DIRECTDRAW7_UNIMPLEMENTED(name)                                \
    do                                                                       \
    {                                                                        \
        static GraphicsCallLedger ledger = {name, kUnimplementedCallBudget};  \
        ReportUnimplementedGraphicsCall("IDirectDraw7", &ledger);             \
    } while (false)

HRESULT WINAPI Dd7Compact(IDirectDraw7*)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("Compact");
    return DD_OK;
}
HRESULT WINAPI Dd7CreateClipper(IDirectDraw7*, DWORD, LPDIRECTDRAWCLIPPER* clipper, IUnknown*)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("CreateClipper");
    if (clipper != nullptr)
    {
        *clipper = nullptr;
    }
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dd7CreatePalette(IDirectDraw7*, DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE* palette, IUnknown*)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("CreatePalette");
    if (palette != nullptr)
    {
        *palette = nullptr;
    }
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dd7DuplicateSurface(IDirectDraw7*, IDirectDrawSurface7*, IDirectDrawSurface7** surface)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("DuplicateSurface");
    if (surface != nullptr)
    {
        *surface = nullptr;
    }
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dd7FlipToGDISurface(IDirectDraw7*)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("FlipToGDISurface");
    return DD_OK;
}
HRESULT WINAPI Dd7GetFourCCCodes(IDirectDraw7*, LPDWORD count, LPDWORD)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("GetFourCCCodes");
    if (count != nullptr)
    {
        *count = 0;
    }
    return DD_OK;
}
HRESULT WINAPI Dd7GetGDISurface(IDirectDraw7*, IDirectDrawSurface7** surface)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("GetGDISurface");
    if (surface != nullptr)
    {
        *surface = nullptr;
    }
    return DDERR_NOTFOUND;
}
HRESULT WINAPI Dd7GetScanLine(IDirectDraw7*, LPDWORD line)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("GetScanLine");
    if (line != nullptr)
    {
        *line = 0;
    }
    return DD_OK;
}
HRESULT WINAPI Dd7GetVerticalBlankStatus(IDirectDraw7*, LPBOOL blank)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("GetVerticalBlankStatus");
    if (blank != nullptr)
    {
        *blank = FALSE;
    }
    return DD_OK;
}
HRESULT WINAPI Dd7Initialize(IDirectDraw7*, GUID*)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("Initialize");
    return DDERR_ALREADYINITIALIZED;
}
HRESULT WINAPI Dd7GetSurfaceFromDC(IDirectDraw7*, HDC, IDirectDrawSurface7** surface)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("GetSurfaceFromDC");
    if (surface != nullptr)
    {
        *surface = nullptr;
    }
    return DDERR_NOTFOUND;
}
HRESULT WINAPI Dd7StartModeTest(IDirectDraw7*, LPSIZE, DWORD, DWORD)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("StartModeTest");
    return DDERR_UNSUPPORTED;
}
HRESULT WINAPI Dd7EvaluateMode(IDirectDraw7*, DWORD, DWORD*)
{
    RE2DJ_DIRECTDRAW7_UNIMPLEMENTED("EvaluateMode");
    return DDERR_UNSUPPORTED;
}

#undef RE2DJ_DIRECTDRAW7_UNIMPLEMENTED

IDirectDraw7Vtbl* DirectDraw7Vtable()
{
    static IDirectDraw7Vtbl table = {};
    static bool initialized = false;
    if (!initialized)
    {
        const IDirectDraw4Vtbl* const legacy = LegacyDirectDrawVtable();

        // Adopted: the DirectX 6 implementation, unchanged.
        Adopt(table.AddRef, legacy->AddRef);
        Adopt(table.Release, legacy->Release);
        Adopt(table.CreateSurface, legacy->CreateSurface);
        Adopt(table.SetCooperativeLevel, legacy->SetCooperativeLevel);
        Adopt(table.SetDisplayMode, legacy->SetDisplayMode);
        Adopt(table.RestoreDisplayMode, legacy->RestoreDisplayMode);
        Adopt(table.RestoreAllSurfaces, legacy->RestoreAllSurfaces);

        // DirectX 7's own slots and the ones it changed.
        table.QueryInterface = Dd7QueryInterface;
        table.GetCaps = Dd7GetCaps;
        table.GetDisplayMode = Dd7GetDisplayMode;
        table.EnumDisplayModes = Dd7EnumDisplayModes;
        table.EnumSurfaces = Dd7EnumSurfaces;
        table.GetMonitorFrequency = Dd7GetMonitorFrequency;
        table.TestCooperativeLevel = Dd7TestCooperativeLevel;
        table.WaitForVerticalBlank = Dd7WaitForVerticalBlank;
        table.GetAvailableVidMem = Dd7GetAvailableVidMem;
        table.GetDeviceIdentifier = Dd7GetDeviceIdentifier;

        // No implementation yet; each records the calls it receives.
        table.Compact = Dd7Compact;
        table.CreateClipper = Dd7CreateClipper;
        table.CreatePalette = Dd7CreatePalette;
        table.DuplicateSurface = Dd7DuplicateSurface;
        table.FlipToGDISurface = Dd7FlipToGDISurface;
        table.GetFourCCCodes = Dd7GetFourCCCodes;
        table.GetGDISurface = Dd7GetGDISurface;
        table.GetScanLine = Dd7GetScanLine;
        table.GetVerticalBlankStatus = Dd7GetVerticalBlankStatus;
        table.Initialize = Dd7Initialize;
        table.GetSurfaceFromDC = Dd7GetSurfaceFromDC;
        table.StartModeTest = Dd7StartModeTest;
        table.EvaluateMode = Dd7EvaluateMode;
        initialized = true;
    }
    return &table;
}

// The DirectX 7 tables seen as the DirectX 6 tables they extend. The shared
// facade stores them without naming DirectX 7 types; the object they are
// installed on is the DirectX 6 object either way.
const IDirectDrawSurface4Vtbl* SurfaceVtableAsLegacy()
{
    return reinterpret_cast<const IDirectDrawSurface4Vtbl*>(Surface7Vtable());
}

const IDirectDraw4Vtbl* RootVtableAsLegacy()
{
    return reinterpret_cast<const IDirectDraw4Vtbl*>(DirectDraw7Vtable());
}

}  // namespace
}  // namespace re2dj::platform::windows

extern "C" __declspec(dllexport) HRESULT WINAPI
Re2djHleDirectDrawCreateEx(GUID* driver_guid,
                           void** direct_draw,
                           REFIID iid,
                           IUnknown* outer)
{
    namespace windows = re2dj::platform::windows;

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

    char msg_buf[176] = {};
    std::snprintf(msg_buf, sizeof(msg_buf),
                  "re2dj:hle:DirectDrawCreateEx driver=%s iid={%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                  driver_text,
                  iid.Data1, iid.Data2, iid.Data3,
                  iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
                  iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);
    windows::WriteGraphicsTraceLine(msg_buf);

    if (direct_draw == nullptr)
    {
        return E_POINTER;
    }
    *direct_draw = nullptr;
    if (outer != nullptr)
    {
        return CLASS_E_NOAGGREGATION;
    }

    // The object is the DirectX 6 root. It is created with the DirectX 7 tables
    // it will install on the surfaces, devices, and vertex buffers it hands
    // out, and then its own table is switched to the DirectX 7 one.
    windows::LegacyFacadeVtables vtables;
    vtables.surface = windows::SurfaceVtableAsLegacy();
    vtables.device = windows::Direct3DDevice7VtableAsLegacy();
    vtables.vertex_buffer = windows::Direct3DVertexBuffer7VtableAsLegacy();

    IDirectDraw4* root = nullptr;
    const HRESULT created = windows::CreateLegacyDirectDrawRoot(vtables, &root);
    if (created != DD_OK)
    {
        return created;
    }
    windows::SetLegacyDirectDrawVtable(root, windows::RootVtableAsLegacy());

    auto* const seven = reinterpret_cast<IDirectDraw7*>(root);
    const HRESULT queried = seven->lpVtbl->QueryInterface(seven, iid, direct_draw);
    seven->lpVtbl->Release(seven);
    return queried;
}
