// ddraw.dll's entry points and IDirectDraw7. The other interfaces the module
// carries have their own files; see ddraw_interfaces.h.

#include "re2dj/hle/modules/ddraw_module.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/directx/directdraw_description.h"
#include "re2dj/directx/directdraw_display.h"
#include "re2dj/directx/directdraw_surface.h"
#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/host_presentation.h"

namespace re2dj::hle::modules
{
namespace
{

namespace dx = re2dj::directx;
using com::CallName;
using com::Fail;
using com::MethodProcess;
using com::Succeed;
using ddraw::DirectDrawState;
using ddraw::kDirectDrawObject;

// The DirectDraw object's state, once MethodProcess has checked the object.
DirectDrawState& StateOf(GuestProcess& process, std::uint32_t direct_draw)
{
    return *process.com().Find(direct_draw)->StateAs<DirectDrawState>();
}

// ---------------------------------------------------------------------------
// DirectDrawEnumerateExA
// ---------------------------------------------------------------------------

// The primary display driver's description on Korean Windows 11,
// "주 디스플레이 드라이버" in code page 949.
constexpr std::string_view kPrimaryDescription =
    "\xC1\xD6 \xB5\xF0\xBD\xBA\xC7\xC3\xB7\xB9\xC0\xCC \xB5\xE5\xB6\xF3\xC0\xCC\xB9\xF6";
constexpr std::string_view kPrimaryName = "display";
// The one monitor's adapter. Windows names the host's graphics card here; the
// guest sees a fixed name instead of one that depends on the machine.
constexpr std::string_view kDisplay1Description = "re2DJ Display Adapter";
constexpr std::string_view kDisplay1Name = "\\\\.\\DISPLAY1";

// One device the enumeration reports, as the callback's arguments.
struct EnumeratedDisplay
{
    bool has_guid = false;
    std::string_view description;
    std::string_view name;
    std::uint32_t monitor = 0;
};

// Calls the guest's enumeration callback for each device: (lpGUID,
// lpDriverDescription, lpDriverName, lpContext) and, for the Ex form,
// hMonitor; a FALSE answer ends the enumeration.
bool EnumerateDisplays(const ImportCall& call,
                       GuestProcess& process,
                       const std::vector<EnumeratedDisplay>& devices,
                       std::uint32_t callback,
                       std::uint32_t context,
                       bool with_monitor,
                       std::string* error)
{
    for (const EnumeratedDisplay& device : devices)
    {
        std::vector<std::uint8_t> bytes(kDisplay1DeviceGuid.begin(), kDisplay1DeviceGuid.end());
        const auto description_offset = static_cast<std::uint32_t>(bytes.size());
        com::AppendText(&bytes, device.description);
        const auto name_offset = static_cast<std::uint32_t>(bytes.size());
        com::AppendText(&bytes, device.name);
        const std::uint32_t block = com::PlaceTemporary(call, process, bytes);
        if (block == 0)
        {
            return Fail(error, "ddraw " + call.gate.name + " cannot place the device strings");
        }
        GuestCall guest_call;
        guest_call.function = callback;
        guest_call.arguments = {device.has_guid ? block : 0U, block + description_offset, block + name_offset,
                                context};
        if (with_monitor)
        {
            guest_call.arguments.push_back(device.monitor);
        }
        std::uint32_t keep_going = 0;
        std::string call_error;
        const bool called = call.services->CallGuest(&guest_call, &keep_going, &call_error);
        process.Free(block);
        if (!called)
        {
            return Fail(error, "ddraw " + call.gate.name + " cannot call the callback: " + call_error);
        }
        if (keep_going == 0)
        {
            break;
        }
    }
    return true;
}

// DirectDrawEnumerateExA(lpCallback, lpContext, dwFlags) for a system with
// one monitor, as Windows 11 enumerates it: the primary display driver (no
// GUID, no monitor), then with DDENUM_ATTACHEDSECONDARYDEVICES the monitor's
// own device. There are no detached or non-display devices.
bool DirectDrawEnumerateExA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 3)
    {
        return Fail(error, result == nullptr ? "ddraw result is null"
                                             : "ddraw DirectDrawEnumerateExA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "ddraw DirectDrawEnumerateExA needs the guest process");
    }
    const std::uint32_t callback = call.arguments[0];
    const std::uint32_t context = call.arguments[1];
    const std::uint32_t flags = call.arguments[2];
    constexpr std::uint32_t kKnownFlags =
        kDdEnumAttachedSecondaryDevices | kDdEnumDetachedSecondaryDevices | kDdEnumNonDisplayDevices;
    if (callback == 0 || (flags & ~kKnownFlags) != 0)
    {
        return Succeed(result, kDdErrInvalidParams, error);
    }

    std::vector<EnumeratedDisplay> devices = {{false, kPrimaryDescription, kPrimaryName, 0}};
    if ((flags & kDdEnumAttachedSecondaryDevices) != 0)
    {
        devices.push_back({true, kDisplay1Description, kDisplay1Name, process->user().PrimaryMonitor()});
    }
    return EnumerateDisplays(call, *process, devices, callback, context, true, error) &&
           Succeed(result, kDdOk, error);
}

// DirectDrawEnumerateA(lpCallback, lpContext), as measured on Windows 11 with
// one monitor: the primary display driver only (no GUID, the same
// description and name as the Ex form), then DD_OK whatever the callback
// answers; a null callback is DDERR_INVALIDPARAMS.
bool DirectDrawEnumerateA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 2)
    {
        return Fail(error, result == nullptr ? "ddraw result is null"
                                             : "ddraw DirectDrawEnumerateA argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "ddraw DirectDrawEnumerateA needs the guest process");
    }
    if (call.arguments[0] == 0)
    {
        return Succeed(result, kDdErrInvalidParams, error);
    }
    const std::vector<EnumeratedDisplay> devices = {{false, kPrimaryDescription, kPrimaryName, 0}};
    return EnumerateDisplays(call, *process, devices, call.arguments[0], call.arguments[1], false, error) &&
           Succeed(result, kDdOk, error);
}

// ---------------------------------------------------------------------------
// IDirectDraw7
// ---------------------------------------------------------------------------

// IDirectDraw7::QueryInterface(this, riid, ppvObj): IUnknown and IDirectDraw7
// answer with the object itself; IDirect3D7 with a new Direct3D object that
// holds a reference to this one, as the Windows facade does. Older
// DirectDraw versions are not modelled and stop, naming the IID.
bool DirectDraw7QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirectDrawObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, dx::kEPointer, error);
    }
    dx::Guid iid{};
    if (!com::ReadGuid(call, call.arguments[1], &iid, error))
    {
        return false;
    }
    std::uint32_t object = 0;
    if (iid == dx::kIidUnknown || iid == dx::kIidDirectDraw7)
    {
        object = call.arguments[0];
        process->com().AddRef(object);
    }
    else if (iid == dx::kIidDirect3D7)
    {
        object = ddraw::CreateDirect3D7(call, *process, call.arguments[0], error);
        if (object == 0)
        {
            return false;
        }
    }
    else
    {
        return Fail(error, CallName(call) + " has no model of " + com::FormatGuid(iid));
    }
    if (!com::WriteWord(call, call.arguments[2], object, error))
    {
        process->com().Release(*process, object);
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::CreateSurface(this, lpDDSurfaceDesc2, lplpDDSurface,
// pUnkOuter) under the shared core's plan: the surfaces it serves, or the
// result it refuses with.
bool DirectDraw7CreateSurface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ddraw::CreateSurfaceOf(call, result, kDirectDrawObject, error);
}

}  // namespace

bool ddraw::CreateSurfaceOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kind, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0 || call.arguments[2] == 0 || call.arguments[3] != 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    dx::DdSurfaceDesc2 request;
    std::array<std::uint8_t, sizeof(dx::DdSurfaceDesc2)> bytes{};
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[1]), bytes, &read_error))
    {
        return Fail(error, CallName(call) + " cannot read DDSURFACEDESC2: " + read_error);
    }
    std::memcpy(&request, bytes.data(), sizeof(request));
    if (!com::WriteWord(call, call.arguments[2], 0, error))
    {
        return false;
    }
    const dx::SurfacePlan plan = dx::PlanCreateSurface(request, StateOf(*process, call.arguments[0]).display);
    // A flipping primary says how the guest presents, even when the request
    // is refused, as the Windows facade records it.
    if (plan.retains_frames && call.services->Presentation() != nullptr)
    {
        call.services->Presentation()->SetRetainBetweenFrames(true);
    }
    if (plan.result != dx::kDdOk)
    {
        return Succeed(result, plan.result, error);
    }
    const std::uint32_t surface =
        ddraw::CreateSurfaces(call, *process, call.arguments[0], plan, kind == ddraw::kDirectDraw4Object, error);
    if (surface == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, call.arguments[2], surface, error))
    {
        process->com().Release(*process, surface);
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

namespace
{

// IDirectDraw7::GetCaps(this, lpDDDriverCaps, lpDDHELCaps): the shared core's
// caps into each structure given.
bool DirectDraw7GetCaps(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 3, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    for (const std::uint32_t caps : {call.arguments[1], call.arguments[2]})
    {
        if (caps != 0 && !com::WriteStruct(call, caps, dx::DirectDraw7Caps(), error))
        {
            return false;
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::EnumDisplayModes(this, dwFlags, lpDDSurfaceDesc2, lpContext,
// lpEnumModesCallback2): every shared-core mode, unfiltered, until the
// callback answers DDENUMRET_CANCEL.
bool DirectDraw7EnumDisplayModes(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 5, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    const std::uint32_t callback = call.arguments[4];
    if (callback == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::array<std::uint32_t, 1> context = {call.arguments[3]};
    for (const dx::DisplayMode& mode : dx::DisplayModes())
    {
        std::uint32_t answer = 0;
        if (!com::CallWithStruct(call, callback, dx::DisplayModeDescription(mode), context, &answer, error))
        {
            return false;
        }
        if (answer == dx::kEnumCancel)
        {
            break;
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::EnumSurfaces(this, dwFlags, lpDDSD2, lpContext, lpCallback):
// the core's plan. The existing surfaces are listed from a snapshot taken
// first, so a callback that releases one does not disturb the walk; one it
// has already released is skipped. Each is AddRef'd for the callback, which
// owns that reference, with its description placed for the length of the
// call.
bool DirectDraw7EnumSurfaces(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 5, kDirectDrawObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t callback = call.arguments[4];
    switch (dx::PlanEnumSurfaces(call.arguments[1], call.arguments[2] != 0, callback != 0))
    {
    case dx::EnumSurfacesPlan::kInvalid:
        return Succeed(result, dx::kDdErrInvalidParams, error);
    case dx::EnumSurfacesPlan::kUnmodelled:
        return Fail(error, CallName(call) + " matching search (flags " + std::to_string(call.arguments[1]) +
                               ") is not modelled");
    case dx::EnumSurfacesPlan::kExisting:
        break;
    }
    for (const std::uint32_t surface : ddraw::ExistingSurfaces(*process, call.arguments[0]))
    {
        dx::DdSurfaceDesc2 description;
        if (!ddraw::DescribeSurface(*process, surface, &description))
        {
            continue;
        }
        process->com().AddRef(surface);
        GuestCall guest_call;
        guest_call.function = callback;
        guest_call.arguments = {surface, 0, call.arguments[3]};
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&description);
        guest_call.data.assign(bytes, bytes + sizeof(description));
        guest_call.data_argument = 1;
        std::uint32_t answer = 0;
        std::string call_error;
        if (!call.services->CallGuest(&guest_call, &answer, &call_error))
        {
            return Fail(error, CallName(call) + " cannot call the callback: " + call_error);
        }
        if (answer == dx::kEnumCancel)
        {
            break;
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::RestoreDisplayMode(this): DD_OK. re2DJ never changes the
// host's display mode, so there is nothing to restore (task 438).
bool DirectDraw7RestoreDisplayMode(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 1, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::RestoreAllSurfaces(this): no surface of the facade is ever
// lost, and with nothing lost Windows 11 answers DD_OK and leaves the last
// error alone.
bool DirectDraw7RestoreAllSurfaces(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 1, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::GetDisplayMode(this, lpDDSurfaceDesc2): the mode the object
// set, 640x480x16 until it sets one.
bool DirectDraw7GetDisplayMode(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kDirectDrawObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const dx::DisplayMode mode = StateOf(*process, call.arguments[0]).display.mode;
    return com::WriteStruct(call, call.arguments[1], dx::DisplayModeDescription(mode), error) &&
           Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::SetCooperativeLevel(this, hWnd, dwFlags) under the shared
// core's rules. The host's policy requires the guest window to exist, as the
// Windows policy does, and then shows it through the host's presentation
// when there is one. A host that cannot show the window stops the run rather
// than tell the guest DDERR_GENERIC for a failure of the host's own.
bool DirectDraw7SetCooperativeLevel(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ddraw::SetCooperativeLevelOf(call, result, kDirectDrawObject, error);
}

// IDirectDraw7::SetDisplayMode(this, width, height, bpp, refresh, flags)
// under the shared core's rules.
bool DirectDraw7SetDisplayMode(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ddraw::SetDisplayModeOf(call, result, kDirectDrawObject, error);
}

}  // namespace

bool ddraw::SetCooperativeLevelOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kind, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestUser& user = process->user();
    HostPresentation* presentation = call.services->Presentation();
    std::string host_error;
    const std::uint32_t hr = dx::SetCooperativeLevel(
        &StateOf(*process, call.arguments[0]).display, call.arguments[1], call.arguments[2],
        [&](std::uint32_t window, const dx::DisplayMode& mode) {
            if (user.LookupWindow(window) == nullptr)
            {
                return false;
            }
            return presentation == nullptr ||
                   presentation->ShowGuestWindow(window, mode.width, mode.height, &host_error);
        });
    if (!host_error.empty())
    {
        return Fail(error, CallName(call) + " cannot show the window on the host: " + host_error);
    }
    return Succeed(result, hr, error);
}

bool ddraw::SetDisplayModeOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 6, kind, error);
    if (process == nullptr)
    {
        return false;
    }
    const dx::DisplayMode mode = {call.arguments[1], call.arguments[2], call.arguments[3]};
    return Succeed(result, dx::SetDisplayMode(&StateOf(*process, call.arguments[0]).display, mode), error);
}

namespace
{

bool DirectDraw7GetMonitorFrequency(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 2, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    return com::WriteWord(call, call.arguments[1], dx::kMonitorFrequency, error) &&
           Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7::GetAvailableVidMem(this, lpDDSCaps2, lpdwTotal, lpdwFree):
// the shared core's fixed budget, whatever the caps.
bool DirectDraw7GetAvailableVidMem(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 4, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    for (const std::uint32_t address : {call.arguments[2], call.arguments[3]})
    {
        if (address != 0 && !com::WriteWord(call, address, dx::kReportedVideoMemory, error))
        {
            return false;
        }
    }
    return Succeed(result, dx::kDdOk, error);
}

bool DirectDraw7GetDeviceIdentifier(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 3, kDirectDrawObject, error) == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    return com::WriteStruct(call, call.arguments[1], dx::DeviceIdentifier(), error) &&
           Succeed(result, dx::kDdOk, error);
}

// IDirectDraw7 in vtable order (ddraw.h).
constexpr com::Method kDirectDraw7Methods[] = {
    {"QueryInterface", 3, &DirectDraw7QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"Compact", 1, &UnimplementedExport},
    {"CreateClipper", 4, &UnimplementedExport},
    {"CreatePalette", 5, &UnimplementedExport},
    {"CreateSurface", 4, &DirectDraw7CreateSurface},
    {"DuplicateSurface", 3, &UnimplementedExport},
    {"EnumDisplayModes", 5, &DirectDraw7EnumDisplayModes},
    {"EnumSurfaces", 5, &DirectDraw7EnumSurfaces},
    {"FlipToGDISurface", 1, &UnimplementedExport},
    {"GetCaps", 3, &DirectDraw7GetCaps},
    {"GetDisplayMode", 2, &DirectDraw7GetDisplayMode},
    {"GetFourCCCodes", 3, &UnimplementedExport},
    {"GetGDISurface", 2, &UnimplementedExport},
    {"GetMonitorFrequency", 2, &DirectDraw7GetMonitorFrequency},
    {"GetScanLine", 2, &UnimplementedExport},
    {"GetVerticalBlankStatus", 2, &UnimplementedExport},
    {"Initialize", 2, &UnimplementedExport},
    {"RestoreDisplayMode", 1, &DirectDraw7RestoreDisplayMode},
    {"SetCooperativeLevel", 3, &DirectDraw7SetCooperativeLevel},
    {"SetDisplayMode", 6, &DirectDraw7SetDisplayMode},
    {"WaitForVerticalBlank", 3, &UnimplementedExport},
    {"GetAvailableVidMem", 4, &DirectDraw7GetAvailableVidMem},
    {"GetSurfaceFromDC", 3, &UnimplementedExport},
    {"RestoreAllSurfaces", 1, &DirectDraw7RestoreAllSurfaces},
    {"TestCooperativeLevel", 1, &UnimplementedExport},
    {"GetDeviceIdentifier", 3, &DirectDraw7GetDeviceIdentifier},
    {"StartModeTest", 4, &UnimplementedExport},
    {"EvaluateMode", 3, &UnimplementedExport},
};

// DirectDrawCreateEx(lpGUID, lplpDD, iid, pUnkOuter) for the primary display
// (NULL) or the one monitor's device, giving an IDirectDraw7. Another
// interface is DDERR_INVALIDPARAMS, as the ddraw.h contract says; the
// DDCREATE_* pseudo-GUIDs, other devices, and aggregation are not modelled.
bool DirectDrawCreateEx(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        return Fail(error, result == nullptr ? "ddraw result is null"
                                             : "ddraw DirectDrawCreateEx argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "ddraw DirectDrawCreateEx needs the guest process");
    }
    const std::uint32_t device = call.arguments[0];
    if (device != 0)
    {
        dx::Guid guid{};
        if (device < 0x10000U || !com::ReadGuid(call, device, &guid, error) || guid != kDisplay1DeviceGuid)
        {
            return Fail(error, "ddraw DirectDrawCreateEx has no model of this device");
        }
    }
    if (call.arguments[3] != 0)
    {
        return Fail(error, "ddraw DirectDrawCreateEx has no model of aggregation");
    }
    dx::Guid iid{};
    if (!com::ReadGuid(call, call.arguments[2], &iid, error))
    {
        return false;
    }
    if (iid != dx::kIidDirectDraw7 || call.arguments[1] == 0)
    {
        return Succeed(result, kDdErrInvalidParams, error);
    }
    GuestComObject object;
    object.kind = kDirectDrawObject;
    object.state = std::make_shared<DirectDrawState>();
    const std::uint32_t direct_draw = com::CreateObject(call, *process, ddraw::kModule, ddraw::kDirectDraw7,
                                                        kDirectDraw7Methods, object, error);
    if (direct_draw == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, call.arguments[1], direct_draw, error))
    {
        process->com().Release(*process, direct_draw);
        return false;
    }
    return Succeed(result, kDdOk, error);
}

// DirectDrawCreate(lpGUID, lplpDD, pUnkOuter) for the primary display: the
// DirectX 6 DirectDraw object EZ2DJ 1st asks IDirectDraw4 and IDirect3D3 of,
// as the Windows product's DX6 facade gives it (one object for IDirectDraw
// and IDirectDraw4). A null lplpDD is DDERR_INVALIDPARAMS and aggregation
// CLASS_E_NOAGGREGATION, as the facade answers; a device GUID stops.
bool DirectDrawCreate(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kClassENoAggregation = 0x80040110U;
    if (result == nullptr || call.arguments.size() != 3)
    {
        return Fail(error, result == nullptr ? "ddraw result is null"
                                             : "ddraw DirectDrawCreate argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "ddraw DirectDrawCreate needs the guest process");
    }
    if (call.arguments[0] != 0)
    {
        return Fail(error, "ddraw DirectDrawCreate has no model of a device GUID");
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, kDdErrInvalidParams, error);
    }
    if (!com::WriteWord(call, call.arguments[1], 0, error))
    {
        return false;
    }
    if (call.arguments[2] != 0)
    {
        return Succeed(result, kClassENoAggregation, error);
    }
    const std::uint32_t direct_draw = ddraw::CreateDirectDraw4(call, *process, error);
    if (direct_draw == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, call.arguments[1], direct_draw, error))
    {
        process->com().Release(*process, direct_draw);
        return false;
    }
    return Succeed(result, kDdOk, error);
}

}  // namespace

GuestModuleDescriptor MakeDdrawModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = std::string(ddraw::kModule);
    descriptor.aliases = {"ddraw"};
    descriptor.exports.push_back(com::MakeExport("DirectDrawEnumerateExA", 3, &DirectDrawEnumerateExA));
    descriptor.exports.push_back(com::MakeExport("DirectDrawCreateEx", 4, &DirectDrawCreateEx));
    descriptor.exports.push_back(com::MakeExport("DirectDrawEnumerateA", 2, &DirectDrawEnumerateA));
    descriptor.exports.push_back(com::MakeExport("DirectDrawCreate", 3, &DirectDrawCreate));
    // Interface methods, reached only through the vtables they fill.
    com::AddMethods(&descriptor, ddraw::kDirectDraw7, kDirectDraw7Methods);
    com::AddMethods(&descriptor, ddraw::kDirect3D7, ddraw::Direct3D7Methods());
    com::AddMethods(&descriptor, ddraw::kDirectDrawSurface7, ddraw::DirectDrawSurface7Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3DDevice7, ddraw::Direct3DDevice7Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3DVertexBuffer7, ddraw::Direct3DVertexBuffer7Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3DVertexBuffer, ddraw::Direct3DVertexBufferMethods());
    com::AddMethods(&descriptor, ddraw::kDirectDraw4, ddraw::DirectDraw4Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3D3, ddraw::Direct3D3Methods());
    com::AddMethods(&descriptor, ddraw::kDirectDrawSurface4, ddraw::DirectDrawSurface4Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3DDevice3, ddraw::Direct3DDevice3Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3DViewport3, ddraw::Direct3DViewport3Methods());
    com::AddMethods(&descriptor, ddraw::kDirect3DTexture2, ddraw::Direct3DTexture2Methods());
    return descriptor;
}

}  // namespace re2dj::hle::modules
