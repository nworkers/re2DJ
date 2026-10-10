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
#include <vector>

#include "facade_com.h"
#include "re2dj/directx/direct3d_vertex_buffer.h"
#include "re2dj/directx/directdraw_display.h"
#include "re2dj/directx/directdraw_surface.h"
#include "re2dj/graphics/legacy_texture.h"
#include "re2dj/hle/guest_com.h"

namespace re2dj::hle::modules::ddraw
{

inline constexpr std::string_view kModule = "ddraw.dll";

// GuestComObject::kind of each ddraw facade object.
inline constexpr std::uint32_t kDirectDrawObject = 1;
inline constexpr std::uint32_t kDirect3DObject = 2;
inline constexpr std::uint32_t kSurfaceObject = 3;
inline constexpr std::uint32_t kDeviceObject = 4;
inline constexpr std::uint32_t kVertexBufferObject = 5;
// DirectX 6 objects (ddraw_direct_draw4.cpp), apart from the DirectX 7 ones.
inline constexpr std::uint32_t kDirectDraw4Object = 6;
inline constexpr std::uint32_t kDirect3D3Object = 7;
inline constexpr std::uint32_t kViewportObject = 8;
inline constexpr std::uint32_t kTexture2Object = 9;
// A windowed title's clipper (ddraw_clipper.cpp, #15).
inline constexpr std::uint32_t kClipperObject = 10;

inline constexpr std::string_view kDirectDraw7 = "IDirectDraw7";
inline constexpr std::string_view kDirect3D7 = "IDirect3D7";
inline constexpr std::string_view kDirectDrawSurface7 = "IDirectDrawSurface7";
inline constexpr std::string_view kDirect3DDevice7 = "IDirect3DDevice7";
inline constexpr std::string_view kDirect3DVertexBuffer7 = "IDirect3DVertexBuffer7";
inline constexpr std::string_view kDirect3DVertexBuffer = "IDirect3DVertexBuffer";
inline constexpr std::string_view kDirectDraw4 = "IDirectDraw4";
inline constexpr std::string_view kDirect3D3 = "IDirect3D3";
inline constexpr std::string_view kDirectDrawSurface4 = "IDirectDrawSurface4";
inline constexpr std::string_view kDirect3DDevice3 = "IDirect3DDevice3";
inline constexpr std::string_view kDirect3DViewport3 = "IDirect3DViewport3";
inline constexpr std::string_view kDirect3DTexture2 = "IDirect3DTexture2";
inline constexpr std::string_view kDirectDrawClipper = "IDirectDrawClipper";

// A DirectDraw object's state: the display it drives, and the surface the
// guest presents from (its device's render target), which a full target
// clear aimed at clears what is about to be shown.
struct DirectDrawState final : GuestComState
{
    re2dj::directx::DirectDrawDisplay display;
    std::uint32_t presentation_surface = 0;
};

// SetCooperativeLevel(this, hWnd, dwFlags) and SetDisplayMode(this, width,
// height, bpp, refresh, flags) on a DirectDraw object of the given kind,
// under the shared core's rules, which IDirectDraw7 and IDirectDraw4 share
// as the Windows facades do (ddraw_module.cpp).
bool SetCooperativeLevelOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error);
bool SetDisplayModeOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error);
// CreateSurface(this, lpDDSurfaceDesc2, lplpDDSurface, pUnkOuter) under the
// shared core's plan; a DirectDraw4 object's surfaces are DirectX 6 ones.
bool CreateSurfaceOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error);

// IDirectDrawClipper in vtable order (ddraw_clipper.cpp); CreateClipper(this,
// dwFlags, lplpDDClipper, pUnkOuter) on a DirectDraw object of the given kind;
// and whether an address is one of its clippers.
std::span<const com::Method> DirectDrawClipperMethods();
bool CreateClipperOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error);
bool IsClipper(GuestProcess& process, std::uint32_t address);
// IDirectDraw4 and IDirect3D3 in vtable order (ddraw_direct_draw4.cpp).
std::span<const com::Method> DirectDraw4Methods();
std::span<const com::Method> Direct3D3Methods();
std::span<const com::Method> Direct3DViewport3Methods();

// A DirectX 6 viewport's state: the D3DVIEWPORT2 SetViewport2 last gave it.
struct ViewportState final : GuestComState
{
    re2dj::directx::D3dViewport2 viewport;
};
// A new DirectX 6 DirectDraw object, as DirectDrawCreate gives it; 0 with
// error set on failure.
std::uint32_t CreateDirectDraw4(const ImportCall& call, GuestProcess& process, std::string* error);

// IDirectDrawSurface7 in vtable order (ddraw_surface7.cpp), and
// IDirectDrawSurface4, its first 45 methods, which DirectX 6 surfaces use.
std::span<const com::Method> DirectDrawSurface7Methods();
std::span<const com::Method> DirectDrawSurface4Methods();
// The surfaces of a served CreateSurface plan, each holding a reference to
// direct_draw, with a flipping primary holding its back buffer. The first
// surface's address, or 0 with error set.
std::uint32_t CreateSurfaces(const ImportCall& call,
                             GuestProcess& process,
                             std::uint32_t direct_draw,
                             const re2dj::directx::SurfacePlan& plan,
                             bool directx6,
                             std::string* error);
// The surfaces of direct_draw that still exist, attached ones included,
// newest first, as IDirectDraw7::EnumSurfaces lists them.
std::vector<std::uint32_t> ExistingSurfaces(GuestProcess& process, std::uint32_t direct_draw);
// A surface's description, as GetSurfaceDesc gives it; false when the
// address is not a surface.
bool DescribeSurface(GuestProcess& process, std::uint32_t surface, re2dj::directx::DdSurfaceDesc2* description);

// A facade surface's shape, or null when the address is not one.
const re2dj::directx::SurfaceShape* SurfaceShapeOf(GuestProcess& process, std::uint32_t surface);
// IDirect3DTexture2::Load between two surfaces, as the Windows DX6 facade
// does: surfaces of another DirectDraw object or without texture caps are
// DDERR_INVALIDOBJECT, another size D3DERR_TEXTURE_LOAD_FAILED, a DC the
// guest holds DDERR_SURFACEBUSY; otherwise the rows (padding cleared) and the
// source color key are copied. The HRESULT goes to *answer; false with error
// set only on a guest memory failure.
bool LoadTextureSurface(const ImportCall& call,
                        GuestProcess& process,
                        std::uint32_t destination,
                        std::uint32_t source,
                        std::uint32_t* answer,
                        std::string* error);

// IDirect3DTexture2 in vtable order (ddraw_texture2.cpp): a DirectX 6
// texture surface's second interface, whose references count on the surface.
std::span<const com::Method> Direct3DTexture2Methods();
// A new IDirect3DTexture2 block for surface, which owns it; 0 with error set
// on failure.
std::uint32_t CreateTexture2(const ImportCall& call, GuestProcess& process, std::uint32_t surface, std::string* error);
// The surface behind an IDirect3DTexture2, or 0 when the address is not one.
std::uint32_t SurfaceOfTexture2(GuestProcess& process, std::uint32_t texture);
// A texture view of a surface's pixels for the render backend: a host copy
// taken again whenever the guest has changed the pixels since, named by an
// identity no other surface ever has. False with error on a guest read
// failure.
bool SurfaceTextureView(const ImportCall& call,
                        GuestProcess& process,
                        std::uint32_t surface,
                        graphics::LegacyTextureView* view,
                        std::string* error);
// Fills a surface's pixels with one 5-6-5 color, as a full target clear
// does, and its true-color plane, when it has one or 32-bit colour is
// selected, with true_color (XRGB8888, narrowing to color).
bool FillSurface(const ImportCall& call,
                 GuestProcess& process,
                 std::uint32_t surface,
                 std::uint16_t color,
                 std::uint32_t true_color,
                 std::string* error);

// IDirect3D7 in vtable order (ddraw_direct3d7.cpp).
std::span<const com::Method> Direct3D7Methods();
// A new IDirect3D7 that holds a reference to direct_draw, as the one
// QueryInterface(IID_IDirect3D7) returns; 0 with error set on failure.
std::uint32_t CreateDirect3D7(const ImportCall& call,
                              GuestProcess& process,
                              std::uint32_t direct_draw,
                              std::string* error);

// IDirect3DVertexBuffer7 in vtable order (ddraw_vertex_buffer7.cpp), and
// DirectX 6's IDirect3DVertexBuffer, its first eight methods.
std::span<const com::Method> Direct3DVertexBuffer7Methods();
std::span<const com::Method> Direct3DVertexBufferMethods();
// A new vertex buffer of the description (already checked by the core, with
// its vertex stride) in zeroed guest memory, holding a reference to
// direct3d; a DirectX 6 buffer carries IDirect3DVertexBuffer's vtable. 0 with
// error set on failure.
std::uint32_t CreateVertexBuffer7(const ImportCall& call,
                                  GuestProcess& process,
                                  std::uint32_t direct3d,
                                  const re2dj::directx::D3dVertexBufferDesc& description,
                                  std::uint32_t stride,
                                  bool directx6,
                                  std::string* error);
// What a draw needs of a vertex buffer: its Direct3D object, guest memory,
// vertex stride and count, format, and whether it is locked.
struct VertexBufferView
{
    std::uint32_t direct3d = 0;
    std::uint32_t address = 0;
    std::uint32_t stride = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t fvf = 0;
    bool locked = false;
};
// False when the address is not a vertex buffer.
bool VertexBufferOf(GuestProcess& process, std::uint32_t buffer, VertexBufferView* view);

// IDirect3DDevice7 in vtable order (ddraw_device7.cpp).
std::span<const com::Method> Direct3DDevice7Methods();
// IDirect3DDevice3 in vtable order, sharing DirectX 7's handlers where the
// arguments agree (ddraw_device7.cpp).
std::span<const com::Method> Direct3DDevice3Methods();
// A new IDirect3DDevice7 rendering into render_target, holding references
// to it and to direct_draw, in the shared core's initial state; 0 with error
// set on failure.
std::uint32_t CreateDirect3DDevice7(const ImportCall& call,
                                    GuestProcess& process,
                                    std::uint32_t direct_draw,
                                    std::uint32_t render_target,
                                    bool directx6,
                                    std::string* error);

}  // namespace re2dj::hle::modules::ddraw

#endif  // RE2DJ_HLE_MODULES_DDRAW_INTERFACES_H_
