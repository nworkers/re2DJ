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
#include "re2dj/hle/guest_com.h"

namespace re2dj::hle::modules::ddraw
{

inline constexpr std::string_view kModule = "ddraw.dll";

// GuestComObject::kind of each ddraw facade object.
inline constexpr std::uint32_t kDirectDrawObject = 1;
inline constexpr std::uint32_t kDirect3DObject = 2;

inline constexpr std::string_view kDirectDraw7 = "IDirectDraw7";
inline constexpr std::string_view kDirect3D7 = "IDirect3D7";

// A DirectDraw object's state: the display it drives.
struct DirectDrawState final : GuestComState
{
    re2dj::directx::DirectDrawDisplay display;
};

// IDirect3D7 in vtable order (ddraw_direct3d7.cpp).
std::span<const com::Method> Direct3D7Methods();
// A new IDirect3D7 that holds a reference to direct_draw, as the one
// QueryInterface(IID_IDirect3D7) returns; 0 with error set on failure.
std::uint32_t CreateDirect3D7(const ImportCall& call,
                              GuestProcess& process,
                              std::uint32_t direct_draw,
                              std::string* error);

}  // namespace re2dj::hle::modules::ddraw

#endif  // RE2DJ_HLE_MODULES_DDRAW_INTERFACES_H_
