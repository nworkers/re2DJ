#pragma once

// The boundary the DirectX 7 facade reaches the DirectX 6 implementation
// through.
//
// DirectX defines each interface version as an extension of the earlier one:
// IDirectDraw7 and IDirectDrawSurface7 repeat their version 4 predecessors slot
// for slot and append to them, and IDirect3DDevice7 keeps the same semantics for
// the methods it carries over even though it reorders them. This project takes
// that structure literally. There is one facade object per DirectDraw object,
// and a later interface version is a different vtable installed on that same
// object rather than a wrapper around it, exactly as DirectDraw itself layers
// its interface versions over one driver object.
//
// A slot whose behavior did not change therefore holds the DirectX 6 function
// pointer itself, and only changed or newly added slots get DirectX 7 code.
//
// This is an internal boundary between two facades, not an ABI. The only
// entry points the guest sees stay Re2djHleDirectDrawCreate and
// Re2djHleDirectDrawCreateEx.
//
// Every declaration here names types that exist at DIRECT3D_VERSION 0x0600, so
// a translation unit compiled for either version can include it.

#define NOMINMAX
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

namespace re2dj::platform::windows
{

// The vtables a root installs on the objects it creates. A null member selects
// the DirectX 6 table, which is what Re2djHleDirectDrawCreate passes.
//
// A later version's table is handed over cast to the DirectX 6 pointer type.
// The facade never calls through these pointers itself - it only stores them in
// the object it creates - and the guest calls through the interface pointer it
// was given, so the stored type only has to be wide enough to carry the table.
struct LegacyFacadeVtables
{
    const IDirectDrawSurface4Vtbl* surface = nullptr;
    const IDirect3DDevice3Vtbl* device = nullptr;
    const IDirect3DVertexBufferVtbl* vertex_buffer = nullptr;
};

// Creates the shared root object and returns its DirectDraw interface. A
// DirectX 7 caller reinterprets the result as IDirectDraw7 after installing its
// own table through `vtables`.
HRESULT CreateLegacyDirectDrawRoot(const LegacyFacadeVtables& vtables,
                                   IDirectDraw4** root);

// Installs a root's DirectDraw vtable. The root is created with the DirectX 6
// table so that construction is complete before the caller decides which
// version it hands to the guest.
void SetLegacyDirectDrawVtable(IDirectDraw4* root, const IDirectDraw4Vtbl* vtable);

// The Direct3D interface that belongs to a root, for a later version's facade
// to build its own Direct3D object on.
IDirect3D3* LegacyDirect3DOfRoot(IDirectDraw4* root);

// The DirectX 6 tables. A slot left null in one of these has no DirectX 6
// implementation, so a later version must supply its own rather than adopt it.
const IDirectDraw4Vtbl* LegacyDirectDrawVtable();
const IDirectDrawSurface4Vtbl* LegacyDirectDrawSurfaceVtable();
const IDirect3D3Vtbl* LegacyDirect3DVtable();
const IDirect3DDevice3Vtbl* LegacyDirect3DDeviceVtable();
const IDirect3DVertexBufferVtbl* LegacyDirect3DVertexBufferVtable();

// A surface and its texture interface are two members of one object. DirectX 6
// passes textures as IDirect3DTexture2 and DirectX 7 passes the surface itself,
// so the DirectX 7 device converts between them here before calling the shared
// implementation.
IDirect3DTexture2* LegacyTextureOfSurface(IDirectDrawSurface4* surface);
IDirectDrawSurface4* LegacySurfaceOfTexture(IDirect3DTexture2* texture);

// Viewport state, version-neutral. DirectX 6 keeps it in an IDirect3DViewport3
// object that the guest attaches to the device; DirectX 7 sets it on the device
// directly. Both end up here, and the draw path reads it from one place.
struct LegacyViewportState
{
    DWORD x = 0;
    DWORD y = 0;
    DWORD width = 0;
    DWORD height = 0;
    float min_z = 0.0f;
    float max_z = 1.0f;
};

HRESULT LegacyDeviceSetViewport(IDirect3DDevice3* device,
                                const LegacyViewportState& viewport);
HRESULT LegacyDeviceGetViewport(IDirect3DDevice3* device,
                                LegacyViewportState* viewport);

// Clears the device's render target. DirectX 6 reaches the same work through
// IDirect3DViewport3::Clear2, which the 1st SE guest does not use, so this is
// the only caller today.
HRESULT LegacyDeviceClear(IDirect3DDevice3* device,
                          DWORD rect_count,
                          const D3DRECT* rects,
                          DWORD flags,
                          D3DCOLOR color,
                          float depth,
                          DWORD stencil);

// Render-target accessors. The DirectX 6 table leaves both slots null because
// the 1st SE guest sets its target at device creation and never changes it.
HRESULT LegacyDeviceSetRenderTarget(IDirect3DDevice3* device,
                                    IDirectDrawSurface4* surface,
                                    DWORD flags);
HRESULT LegacyDeviceGetRenderTarget(IDirect3DDevice3* device,
                                    IDirectDrawSurface4** surface);

// Surface pixel access. The DirectX 6 table leaves Lock and Unlock null because
// the 1st SE guest uploads through GetDC and Blt; the 4th guest locks its
// textures, so the shared implementation now exposes the pixels it already
// holds.
HRESULT LegacySurfaceLock(IDirectDrawSurface4* surface,
                          RECT* rect,
                          DDSURFACEDESC2* descriptor,
                          DWORD flags,
                          HANDLE event);
HRESULT LegacySurfaceUnlock(IDirectDrawSurface4* surface, RECT* rect);

}  // namespace re2dj::platform::windows
