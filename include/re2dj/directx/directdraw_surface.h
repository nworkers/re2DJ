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
// IDirectDrawSurface7::GetDC: a surface without pixels has no DC
// (DDERR_UNSUPPORTED), and one whose DC the guest already holds refuses a
// second (DDERR_DCALREADYCREATED). ReleaseDC takes back only the DC it gave,
// while held (else DDERR_INVALIDPARAMS).
std::uint32_t CheckGetDc(bool has_pixels, bool held);
std::uint32_t CheckReleaseDc(bool held, bool same_dc);

// A RECT argument, as the guest passes it.
struct SurfaceRect
{
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

// What IDirectDrawSurface7::Lock answers: the result, the description to
// write, and the byte offset of the locked area's first pixel from the
// surface's pixels (the caller puts that address in description.surface).
struct SurfaceLockPlan
{
    std::uint32_t result = kDdOk;
    DdSurfaceDesc2 description;
    std::uint32_t offset = 0;
};

// IDirectDrawSurface7::Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent),
// as the Windows facade answers it: the surface's own pixels, whatever the
// flags; a render target is not read back from the render backend.
// - no description, one whose dwSize is wrong, or an event:
//   DDERR_INVALIDPARAMS;
// - a surface without pixels: DDERR_INVALIDOBJECT;
// - a rectangle that is empty or leaves the surface: DDERR_INVALIDRECT;
// - otherwise GetSurfaceDesc's description with DDSD_LPSURFACE, the size of
//   the rectangle (or the surface), the whole surface's pitch, and the
//   offset of the rectangle's first pixel.
SurfaceLockPlan PlanLock(const SurfaceShape& shape,
                         std::uint32_t pitch,
                         bool has_pixels,
                         bool description_given,
                         bool has_event,
                         const SurfaceRect* rect);

// IDirectDrawSurface7::SetColorKey: only a source blit key (DDCKEY_SRCBLT)
// is modelled; any other flags, or no key, are DDERR_INVALIDPARAMS.
std::uint32_t CheckSetColorKey(std::uint32_t flags, bool has_key);

enum class AttachmentQuery : std::uint8_t
{
    kNone,
    kBackBuffer,
    kDepth,
};
AttachmentQuery QueryAttachment(const DdsCaps2& caps);

// IDirectDraw7::EnumSurfaces, as Windows 11 answers it:
// - DDENUMSURFACES_ALL | DDENUMSURFACES_DOESEXIST lists every surface of the
//   DirectDraw object that still exists, attached ones included, newest
//   first. Each is AddRef'd for the callback, which owns that reference, and
//   described as GetSurfaceDesc describes it. DDENUMRET_CANCEL stops the
//   list; the result is DD_OK either way;
// - no callback, unknown flags, not exactly one of ALL/MATCH/NOMATCH and one
//   of DOESEXIST/CANBECREATED, ALL with CANBECREATED, or MATCH/NOMATCH
//   without a description are DDERR_INVALIDPARAMS;
// - the matching searches (MATCH/NOMATCH with a description) are not
//   modelled.
enum class EnumSurfacesPlan : std::uint8_t
{
    kInvalid,
    kExisting,
    kUnmodelled,
};
EnumSurfacesPlan PlanEnumSurfaces(std::uint32_t flags, bool has_description, bool has_callback);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECTDRAW_SURFACE_H_
