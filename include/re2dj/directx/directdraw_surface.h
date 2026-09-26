#ifndef RE2DJ_DIRECTX_DIRECTDRAW_SURFACE_H_
#define RE2DJ_DIRECTX_DIRECTDRAW_SURFACE_H_

#include <cstdint>

#include "re2dj/directx/abi.h"
#include "re2dj/directx/directdraw_display.h"

// DirectDraw surfaces as the DirectX HLE models them: which surfaces
// CreateSurface serves and with what shape, what a surface reports about
// itself, and which attachments it takes. Both hosts' facades follow these
// rules; each keeps the pixels its own way.
namespace re2dj::directx
{

enum class SurfaceKind : std::uint8_t
{
    kPrimary,
    kBackBuffer,
    kDepth,
    kTexture,
    kOffscreen,
};

// A surface's shape: its kind, size, depth, and the DDSCAPS it reports.
struct SurfaceShape
{
    SurfaceKind kind = SurfaceKind::kPrimary;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bits_per_pixel = 16;
    std::uint32_t caps = 0;

    // The depth buffer belongs to the render backend, which owns its own
    // depth attachment, so the guest's depth surface carries no pixels.
    bool has_pixels() const { return kind != SurfaceKind::kDepth; }
};

// What CreateSurface makes of a request: the result, the surface, and for a
// flipping primary its one back buffer.
struct SurfacePlan
{
    std::uint32_t result = kDdOk;
    SurfaceShape surface;
    bool has_back_buffer = false;
    SurfaceShape back_buffer;
    // Set by a primary with DDSCAPS_FLIP, whether or not the request is
    // served: such a guest presents by handing over buffers it owns, so a
    // frame starts from what the last one left. One that copies a whole
    // surface onto a non-flipping primary starts from nothing.
    bool retains_frames = false;
};

// IDirectDraw7::CreateSurface's rules for a DDSURFACEDESC2:
// - a wrong dwSize is DDERR_INVALIDPARAMS;
// - DDSCAPS_ZBUFFER: a depth surface, the display's size unless given;
// - DDSCAPS_TEXTURE: needs caps, size, and an RGB565 pixel format, or it is
//   DDERR_INVALIDPIXELFORMAT;
// - DDSCAPS_OFFSCREENPLAIN: needs caps and size, and RGB565 when a pixel
//   format is given, or it is DDERR_INVALIDPIXELFORMAT;
// - DDSCAPS_PRIMARYSURFACE with no or one back buffer: the display's size and
//   depth; a back buffer makes it COMPLEX | FLIP and adds BACKBUFFER |
//   3DDEVICE; a requested 3DDEVICE carries over, as the 4th asks the primary
//   itself to be the render target;
// - anything else is DDERR_UNSUPPORTED.
SurfacePlan PlanCreateSurface(const DdSurfaceDesc2& request, const DirectDrawDisplay& display);

// The one pixel layout the shared surface backing stores.
bool IsRgb565Format(const DdPixelFormat& format);
// Bytes per RGB565 row, rounded up to four bytes like a DIB row.
std::uint32_t Rgb565Pitch(std::uint32_t width);

// IDirectDrawSurface7::GetSurfaceDesc: caps, size, pitch, and the RGB565
// format, whatever the kind; a depth surface's pitch is 0, as it has no
// pixels.
DdSurfaceDesc2 SurfaceDescription(const SurfaceShape& shape, std::uint32_t pitch);

// IDirectDrawSurface7::AddAttachedSurface: only a depth surface attaches;
// anything else is DDERR_CANNOTATTACHSURFACE.
std::uint32_t CheckAttachment(const SurfaceShape& attachment);

// Which attachment IDirectDrawSurface7::GetAttachedSurface asks for.
enum class AttachmentQuery : std::uint8_t
{
    kNone,
    kBackBuffer,
    kDepth,
};
AttachmentQuery QueryAttachment(const DdsCaps2& caps);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECTDRAW_SURFACE_H_
