// IDirectDraw4 and IDirect3D3: the DirectX 6 interfaces EZ2DJ 1st reaches
// through DirectDrawCreate and QueryInterface, as the Windows product's DX6
// facade (direct3d3_com_facade.cpp) answers them. Methods not modelled yet
// stop, naming themselves.

#include <array>
#include <string>

#include "ddraw_interfaces.h"
#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/direct3d_device.h"
#include "re2dj/directx/direct3d_vertex_buffer.h"

namespace re2dj::hle::modules::ddraw
{
namespace
{

namespace dx = re2dj::directx;
using com::Fail;
using com::MethodProcess;
using com::Succeed;

// QueryInterface answered for both DX6 interfaces, as the Windows facade
// answers them for its one root object: IUnknown, IDirectDraw, and
// IDirectDraw4 give the DirectDraw object, IDirect3D3 a Direct3D object on
// it. Other interfaces stop.
bool QueryDirectX6(const ImportCall& call,
                   ImportReturn* result,
                   GuestProcess& process,
                   std::uint32_t direct_draw,
                   std::string* error)
{
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
    if (iid == dx::kIidUnknown || iid == dx::kIidDirectDraw || iid == dx::kIidDirectDraw4)
    {
        object = direct_draw;
        process.com().AddRef(object);
    }
    else if (iid == dx::kIidDirect3D3)
    {
        GuestComObject direct3d;
        direct3d.kind = kDirect3D3Object;
        direct3d.parent = direct_draw;
        object = com::CreateObject(call, process, kModule, kDirect3D3, Direct3D3Methods(), direct3d, error);
        if (object == 0)
        {
            return false;
        }
        // The Direct3D interface keeps the DirectDraw object alive, as the
        // Windows facade's shared root does.
        process.com().AddRef(direct_draw);
    }
    else
    {
        return Fail(error, com::CallName(call) + " has no model of " + com::FormatGuid(iid));
    }
    if (!com::WriteWord(call, call.arguments[2], object, error))
    {
        process.com().Release(process, object);
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

bool DirectDraw4QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirectDraw4Object, error);
    return process != nullptr && QueryDirectX6(call, result, *process, call.arguments[0], error);
}

bool Direct3D3QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirect3D3Object, error);
    if (process == nullptr)
    {
        return false;
    }
    const GuestComObject* direct3d = process->com().Find(call.arguments[0]);
    return direct3d != nullptr && QueryDirectX6(call, result, *process, direct3d->parent, error);
}

// IDirect3D3::FindDevice(this, lpD3DFDS, lpD3DFDR) under the shared core, as
// the Windows DX6 facade answers; a null search or result is
// DDERR_INVALIDPARAMS.
bool Direct3D3FindDevice(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 3, kDirect3D3Object, error) == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0 || call.arguments[2] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    dx::D3dFindDeviceSearch search;
    dx::D3dFindDeviceResult found;
    if (!com::ReadStruct(call, call.arguments[1], &search, error) ||
        !com::ReadStruct(call, call.arguments[2], &found, error))
    {
        return false;
    }
    const std::uint32_t hr = dx::FindDevice(search, &found);
    if (hr == dx::kDdOk && !com::WriteStruct(call, call.arguments[2], found, error))
    {
        return false;
    }
    return Succeed(result, hr, error);
}

// IDirect3D3::EnumZBufferFormats(this, riidDevice, callback, lpContext)
// under the shared core: the HAL class's one 16-bit format, the callback's
// answer not consulted, as the Windows DX6 facade does.
bool Direct3D3EnumZBufferFormats(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 4, kDirect3D3Object, error) == nullptr)
    {
        return false;
    }
    dx::Guid device_class{};
    if (call.arguments[1] == 0 || call.arguments[2] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (!com::ReadGuid(call, call.arguments[1], &device_class, error))
    {
        return false;
    }
    if (dx::CheckEnumZBufferFormats3(device_class) != dx::kDdOk)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::array<std::uint32_t, 1> context = {call.arguments[3]};
    std::uint32_t answer = 0;
    return com::CallWithStruct(call, call.arguments[2], dx::Direct3D3DepthFormat(), context, &answer, error) &&
           Succeed(result, dx::kDdOk, error);
}

// IDirect3D3::CreateDevice(this, rclsid, lpDDS, lplpD3DDevice, pUnkOuter),
// as the Windows DX6 facade checks it: a null out pointer or render target,
// or aggregation, is DDERR_INVALIDPARAMS; the out pointer is cleared; a class
// the enumeration does not know, or a target that is no surface, is
// DDERR_INVALIDOBJECT; then the shared core's device rules. The device keeps
// the DirectDraw object and its render target alive.
bool Direct3D3CreateDevice(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 5, kDirect3D3Object, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t render_target = call.arguments[2];
    const std::uint32_t out = call.arguments[3];
    if (out == 0 || render_target == 0 || call.arguments[4] != 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (!com::WriteWord(call, out, 0, error))
    {
        return false;
    }
    dx::Guid device_class{};
    if (!com::ReadGuid(call, call.arguments[1], &device_class, error))
    {
        return false;
    }
    const dx::SurfaceShape* target = SurfaceShapeOf(*process, render_target);
    if (!dx::IsEnumeratedDevice(device_class) || target == nullptr)
    {
        return Succeed(result, dx::kDdErrInvalidObject, error);
    }
    const std::uint32_t checked = dx::CheckCreateDevice(device_class, target->caps);
    if (checked != dx::kDdOk)
    {
        return Succeed(result, checked, error);
    }
    const std::uint32_t direct_draw = process->com().Find(call.arguments[0])->parent;
    const std::uint32_t device = CreateDirect3DDevice7(call, *process, direct_draw, render_target, true, error);
    if (device == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, out, device, error))
    {
        process->com().Release(*process, device);
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3D3::CreateVertexBuffer(this, lpVBDesc, lplpD3DVertexBuffer,
// dwFlags, pUnkOuter), as the Windows DX6 facade answers: aggregation is
// DDERR_INVALIDPARAMS with the out pointer untouched; otherwise the out
// pointer is cleared and the core's checks decide. The buffer is DirectX 7's
// with IDirect3DVertexBuffer's vtable.
bool Direct3D3CreateVertexBuffer(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 5, kDirect3D3Object, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[4] != 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const std::uint32_t out = call.arguments[2];
    dx::D3dVertexBufferDesc description;
    if (call.arguments[1] != 0 && !com::ReadStruct(call, call.arguments[1], &description, error))
    {
        return false;
    }
    std::uint32_t stride = 0;
    const std::uint32_t checked = dx::CheckCreateVertexBuffer(out != 0, call.arguments[1] != 0, description, &stride);
    if (out != 0 && !com::WriteWord(call, out, 0, error))
    {
        return false;
    }
    if (checked != dx::kDdOk)
    {
        return Succeed(result, checked, error);
    }
    const std::uint32_t buffer = CreateVertexBuffer7(call, *process, call.arguments[0], description, stride, true, error);
    if (buffer == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, out, buffer, error))
    {
        process->com().Release(*process, buffer);
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3D3::CreateViewport(this, lplpD3DViewport, pUnkOuter), as the
// Windows DX6 facade makes one: a null out pointer or aggregation is
// DDERR_INVALIDPARAMS; a new viewport holds a zeroed D3DVIEWPORT2 with its
// size set, and keeps the DirectDraw object alive.
bool Direct3D3CreateViewport(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirect3D3Object, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t out = call.arguments[1];
    if (out == 0 || call.arguments[2] != 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (!com::WriteWord(call, out, 0, error))
    {
        return false;
    }
    auto state = std::make_shared<ViewportState>();
    state->viewport.size = sizeof(dx::D3dViewport2);
    GuestComObject object;
    object.kind = kViewportObject;
    object.parent = process->com().Find(call.arguments[0])->parent;
    object.state = state;
    const std::uint32_t viewport =
        com::CreateObject(call, *process, kModule, kDirect3DViewport3, Direct3DViewport3Methods(), object, error);
    if (viewport == 0)
    {
        return false;
    }
    process->com().AddRef(object.parent);
    if (!com::WriteWord(call, out, viewport, error))
    {
        process->com().Release(*process, viewport);
        return false;
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3DViewport3::QueryInterface: the viewport itself for IUnknown and
// IDirect3DViewport3, E_NOINTERFACE with the out pointer cleared otherwise,
// as the facade answers.
bool ViewportQueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kENoInterface = 0x80004002U;
    GuestProcess* process = MethodProcess(call, result, 3, kViewportObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, dx::kEPointer, error);
    }
    dx::Guid iid{};
    if (!com::WriteWord(call, call.arguments[2], 0, error) || !com::ReadGuid(call, call.arguments[1], &iid, error))
    {
        return false;
    }
    if (iid != dx::kIidUnknown && iid != dx::kIidDirect3DViewport3)
    {
        return Succeed(result, kENoInterface, error);
    }
    if (!com::WriteWord(call, call.arguments[2], call.arguments[0], error))
    {
        return false;
    }
    process->com().AddRef(call.arguments[0]);
    return Succeed(result, dx::kDdOk, error);
}

// GetViewport2 / SetViewport2(this, lpData): a null structure or one whose
// dwSize is not D3DVIEWPORT2's is DDERR_INVALIDPARAMS; otherwise the whole
// structure is copied, as the facade does.
bool ViewportGetViewport2(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kViewportObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    std::uint32_t size = 0;
    if (!com::ReadStruct(call, call.arguments[1], &size, error))
    {
        return false;
    }
    if (size != sizeof(dx::D3dViewport2))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const auto* state = process->com().Find(call.arguments[0])->StateAs<ViewportState>();
    return com::WriteStruct(call, call.arguments[1], state->viewport, error) && Succeed(result, dx::kDdOk, error);
}

bool ViewportSetViewport2(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kViewportObject, error);
    if (process == nullptr)
    {
        return false;
    }
    dx::D3dViewport2 viewport;
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (!com::ReadStruct(call, call.arguments[1], &viewport, error))
    {
        return false;
    }
    if (viewport.size != sizeof(dx::D3dViewport2))
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    process->com().Find(call.arguments[0])->StateAs<ViewportState>()->viewport = viewport;
    return Succeed(result, dx::kDdOk, error);
}

// IDirect3DViewport3 in vtable order (d3d.h).
constexpr com::Method kViewport3Methods[] = {
    {"QueryInterface", 3, &ViewportQueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"Initialize", 2, &UnimplementedExport},
    {"GetViewport", 2, &UnimplementedExport},
    {"SetViewport", 2, &UnimplementedExport},
    {"TransformVertices", 5, &UnimplementedExport},
    {"LightElements", 3, &UnimplementedExport},
    {"SetBackground", 2, &UnimplementedExport},
    {"GetBackground", 3, &UnimplementedExport},
    {"SetBackgroundDepth", 2, &UnimplementedExport},
    {"GetBackgroundDepth", 3, &UnimplementedExport},
    {"Clear", 4, &UnimplementedExport},
    {"AddLight", 2, &UnimplementedExport},
    {"DeleteLight", 2, &UnimplementedExport},
    {"NextLight", 4, &UnimplementedExport},
    {"GetViewport2", 2, &ViewportGetViewport2},
    {"SetViewport2", 2, &ViewportSetViewport2},
    {"SetBackgroundDepth2", 2, &UnimplementedExport},
    {"GetBackgroundDepth2", 3, &UnimplementedExport},
    {"Clear2", 7, &UnimplementedExport},
};

bool DirectDraw4CreateSurface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return CreateSurfaceOf(call, result, kDirectDraw4Object, error);
}

bool DirectDraw4SetCooperativeLevel(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return SetCooperativeLevelOf(call, result, kDirectDraw4Object, error);
}

bool DirectDraw4SetDisplayMode(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return SetDisplayModeOf(call, result, kDirectDraw4Object, error);
}

// IDirectDraw4::RestoreAllSurfaces(this): DD_OK, as the Windows DX6 facade
// answers and as IDirectDraw7's does; no facade surface is ever lost.
bool DirectDraw4RestoreAllSurfaces(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 1, kDirectDraw4Object, error) != nullptr &&
           Succeed(result, dx::kDdOk, error);
}

// IDirectDraw4 in vtable order (ddraw.h).
constexpr com::Method kDirectDraw4Methods[] = {
    {"QueryInterface", 3, &DirectDraw4QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"Compact", 1, &UnimplementedExport},
    {"CreateClipper", 4, &UnimplementedExport},
    {"CreatePalette", 5, &UnimplementedExport},
    {"CreateSurface", 4, &DirectDraw4CreateSurface},
    {"DuplicateSurface", 3, &UnimplementedExport},
    {"EnumDisplayModes", 5, &UnimplementedExport},
    {"EnumSurfaces", 5, &UnimplementedExport},
    {"FlipToGDISurface", 1, &UnimplementedExport},
    {"GetCaps", 3, &UnimplementedExport},
    {"GetDisplayMode", 2, &UnimplementedExport},
    {"GetFourCCCodes", 3, &UnimplementedExport},
    {"GetGDISurface", 2, &UnimplementedExport},
    {"GetMonitorFrequency", 2, &UnimplementedExport},
    {"GetScanLine", 2, &UnimplementedExport},
    {"GetVerticalBlankStatus", 2, &UnimplementedExport},
    {"Initialize", 2, &UnimplementedExport},
    {"RestoreDisplayMode", 1, &UnimplementedExport},
    {"SetCooperativeLevel", 3, &DirectDraw4SetCooperativeLevel},
    {"SetDisplayMode", 6, &DirectDraw4SetDisplayMode},
    {"WaitForVerticalBlank", 3, &UnimplementedExport},
    {"GetAvailableVidMem", 4, &UnimplementedExport},
    {"GetSurfaceFromDC", 3, &UnimplementedExport},
    {"RestoreAllSurfaces", 1, &DirectDraw4RestoreAllSurfaces},
    {"TestCooperativeLevel", 1, &UnimplementedExport},
    {"GetDeviceIdentifier", 3, &UnimplementedExport},
};

// IDirect3D3 in vtable order (d3d.h).
constexpr com::Method kDirect3D3Methods[] = {
    {"QueryInterface", 3, &Direct3D3QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"EnumDevices", 3, &UnimplementedExport},
    {"CreateLight", 3, &UnimplementedExport},
    {"CreateMaterial", 3, &UnimplementedExport},
    {"CreateViewport", 3, &Direct3D3CreateViewport},
    {"FindDevice", 3, &Direct3D3FindDevice},
    {"CreateDevice", 5, &Direct3D3CreateDevice},
    {"CreateVertexBuffer", 5, &Direct3D3CreateVertexBuffer},
    {"EnumZBufferFormats", 4, &Direct3D3EnumZBufferFormats},
    {"EvictManagedTextures", 1, &UnimplementedExport},
};

}  // namespace

std::span<const com::Method> DirectDraw4Methods()
{
    return kDirectDraw4Methods;
}

std::span<const com::Method> Direct3D3Methods()
{
    return kDirect3D3Methods;
}

std::span<const com::Method> Direct3DViewport3Methods()
{
    return kViewport3Methods;
}

std::uint32_t CreateDirectDraw4(const ImportCall& call, GuestProcess& process, std::string* error)
{
    GuestComObject object;
    object.kind = kDirectDraw4Object;
    object.state = std::make_shared<DirectDrawState>();
    return com::CreateObject(call, process, kModule, kDirectDraw4, kDirectDraw4Methods, object, error);
}

}  // namespace re2dj::hle::modules::ddraw
