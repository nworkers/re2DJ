// IDirect3DTexture2 of the ddraw facade: the second interface of a DirectX 6
// texture surface, as the Windows DX6 facade gives it. Its block is owned by
// the surface, and the guest's references on it count on the surface.

#include <cstdint>
#include <memory>
#include <string>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_process.h"

namespace re2dj::hle::modules::ddraw
{
namespace
{

namespace dx = re2dj::directx;
using com::CallName;
using com::Fail;
using com::MethodProcess;
using com::Succeed;

// The surface a texture block belongs to.
struct Texture2State final : GuestComState
{
    std::uint32_t surface = 0;
};

std::uint32_t SurfaceOf(GuestProcess& process, std::uint32_t texture)
{
    const GuestComObject* object = process.com().Find(texture);
    const Texture2State* state = object == nullptr ? nullptr : object->StateAs<Texture2State>();
    return state == nullptr ? 0 : state->surface;
}

// QueryInterface(this, riid, ppvObj): the surface's answer, as the Windows
// facade forwards it: the texture for IID_IDirect3DTexture2, the surface for
// IUnknown and IDirectDrawSurface4.
bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kTexture2Object, error);
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
    const std::uint32_t surface = SurfaceOf(*process, call.arguments[0]);
    std::uint32_t answer = 0;
    if (iid == dx::kIidDirect3DTexture2)
    {
        answer = call.arguments[0];
    }
    else if (iid == dx::kIidUnknown || iid == dx::kIidDirectDrawSurface4)
    {
        answer = surface;
    }
    else
    {
        return Fail(error, CallName(call) + " has no model of " + com::FormatGuid(iid));
    }
    if (!com::WriteWord(call, call.arguments[2], answer, error))
    {
        return false;
    }
    process->com().AddRef(surface);
    return Succeed(result, dx::kDdOk, error);
}

// AddRef(this) and Release(this): the surface's count, which the texture
// shares.
bool AddRef(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kTexture2Object, error);
    if (process == nullptr)
    {
        return false;
    }
    return Succeed(result, process->com().AddRef(SurfaceOf(*process, call.arguments[0])), error);
}

bool Release(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 1, kTexture2Object, error);
    if (process == nullptr)
    {
        return false;
    }
    return Succeed(result, process->com().Release(*process, SurfaceOf(*process, call.arguments[0])), error);
}

// GetHandle(this, lpDirect3DDevice2, lpHandle) and PaletteChanged(this,
// dwStart, dwCount): DDERR_UNSUPPORTED, as in the Windows facade.
bool GetHandle(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kTexture2Object, error);
    return process != nullptr && Succeed(result, dx::kDdErrUnsupported, error);
}

bool PaletteChanged(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kTexture2Object, error);
    return process != nullptr && Succeed(result, dx::kDdErrUnsupported, error);
}

// Load(this, lpD3DTexture2): NULL is DDERR_INVALIDPARAMS, itself DD_OK, and
// anything that is no texture DDERR_INVALIDOBJECT; two textures follow the
// surface copy rules.
bool Load(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kTexture2Object, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t source = call.arguments[1];
    if (source == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    if (source == call.arguments[0])
    {
        return Succeed(result, dx::kDdOk, error);
    }
    const std::uint32_t source_surface = SurfaceOf(*process, source);
    if (source_surface == 0)
    {
        return Succeed(result, dx::kDdErrInvalidObject, error);
    }
    std::uint32_t answer = 0;
    if (!LoadTextureSurface(call, *process, SurfaceOf(*process, call.arguments[0]), source_surface, &answer, error))
    {
        return false;
    }
    return Succeed(result, answer, error);
}

constexpr com::Method kMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &AddRef},
    {"Release", 1, &Release},
    {"GetHandle", 3, &GetHandle},
    {"PaletteChanged", 3, &PaletteChanged},
    {"Load", 2, &Load},
};

}  // namespace

std::span<const com::Method> Direct3DTexture2Methods()
{
    return kMethods;
}

std::uint32_t CreateTexture2(const ImportCall& call, GuestProcess& process, std::uint32_t surface, std::string* error)
{
    auto state = std::make_shared<Texture2State>();
    state->surface = surface;
    GuestComObject object;
    object.kind = kTexture2Object;
    object.state = state;
    return com::CreateObject(call, process, kModule, kDirect3DTexture2, kMethods, object, error);
}

std::uint32_t SurfaceOfTexture2(GuestProcess& process, std::uint32_t texture)
{
    return SurfaceOf(process, texture);
}

}  // namespace re2dj::hle::modules::ddraw
