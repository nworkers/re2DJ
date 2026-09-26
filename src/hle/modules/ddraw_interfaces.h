#ifndef RE2DJ_HLE_MODULES_DDRAW_INTERFACES_H_
#define RE2DJ_HLE_MODULES_DDRAW_INTERFACES_H_

// The COM interfaces of the ddraw facade, one file each, as the Windows facade
// divides them. They share one module because a DirectX 7 title reaches
// Direct3D only through QueryInterface on a DirectDraw object: ddraw.dll
// carries it, and the guest imports no Direct3D DLL. Internal to
// src/hle/modules.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "facade_com.h"
#include "re2dj/directx/directdraw_display.h"
#include "re2dj/directx/directdraw_surface.h"
#include "re2dj/hle/guest_com.h"

namespace re2dj::hle::modules::ddraw
{

inline constexpr std::string_view kModule = "ddraw.dll";

// GuestComObject::kind of each ddraw facade object.
inline constexpr std::uint32_t kDirectDrawObject = 1;
inline constexpr std::uint32_t kDirect3DObject = 2;
inline constexpr std::uint32_t kSurfaceObject = 3;
inline constexpr std::uint32_t kDeviceObject = 4;

inline constexpr std::string_view kDirectDraw7 = "IDirectDraw7";
inline constexpr std::string_view kDirect3D7 = "IDirect3D7";
inline constexpr std::string_view kDirectDrawSurface7 = "IDirectDrawSurface7";
inline constexpr std::string_view kDirect3DDevice7 = "IDirect3DDevice7";

// A DirectDraw object's state: the display it drives.
struct DirectDrawState final : GuestComState
{
    re2dj::directx::DirectDrawDisplay display;
};

// IDirectDrawSurface7 in vtable order (ddraw_surface7.cpp).
std::span<const com::Method> DirectDrawSurface7Methods();
// The surfaces of a served CreateSurface plan, each holding a reference to
// direct_draw, with a flipping primary holding its back buffer. The first
// surface's address, or 0 with error set.
std::uint32_t CreateSurfaces(const ImportCall& call,
                             GuestProcess& process,
                             std::uint32_t direct_draw,
                             const re2dj::directx::SurfacePlan& plan,
                             std::string* error);
// A facade surface's shape, or null when the address is not one.
const re2dj::directx::SurfaceShape* SurfaceShapeOf(GuestProcess& process, std::uint32_t surface);

// IDirect3D7 in vtable order (ddraw_direct3d7.cpp).
std::span<const com::Method> Direct3D7Methods();
// A new IDirect3D7 that holds a reference to direct_draw, as the one
// QueryInterface(IID_IDirect3D7) returns; 0 with error set on failure.
std::uint32_t CreateDirect3D7(const ImportCall& call,
                              GuestProcess& process,
                              std::uint32_t direct_draw,
                              std::string* error);

// IDirect3DDevice7 in vtable order (ddraw_device7.cpp).
std::span<const com::Method> Direct3DDevice7Methods();
// A new IDirect3DDevice7 rendering into render_target, holding references
// to it and to direct_draw, in the shared core's initial state; 0 with error
// set on failure.
std::uint32_t CreateDirect3DDevice7(const ImportCall& call,
                                    GuestProcess& process,
                                    std::uint32_t direct_draw,
                                    std::uint32_t render_target,
                                    std::string* error);

}  // namespace re2dj::hle::modules::ddraw

#endif  // RE2DJ_HLE_MODULES_DDRAW_INTERFACES_H_
