#include "re2dj/directx/directdraw_surface.h"

#include "re2dj/directx/direct3d_description.h"

namespace re2dj::directx
{

SurfacePlan PlanCreateSurface(const DdSurfaceDesc2& request, const DirectDrawDisplay& display)
{
    SurfacePlan plan;
    if (request.size != sizeof(DdSurfaceDesc2))
    {
        plan.result = kDdErrInvalidParams;
        return plan;
    }
    const std::uint32_t caps = request.caps.caps;
    plan.retains_frames = (caps & kDdsCapsPrimarySurface) != 0 && (caps & kDdsCapsFlip) != 0;
    SurfaceShape& shape = plan.surface;

    if ((caps & kDdsCapsZBuffer) != 0)
    {
        shape.kind = SurfaceKind::kDepth;
        shape.width = request.width != 0 ? request.width : display.mode.width;
        shape.height = request.height != 0 ? request.height : display.mode.height;
        shape.caps = caps;
        return plan;
    }
    if ((caps & kDdsCapsTexture) != 0)
    {
        constexpr std::uint32_t kRequired = kDdsdCaps | kDdsdWidth | kDdsdHeight | kDdsdPixelFormat;
        if ((request.flags & kRequired) != kRequired || request.width == 0 || request.height == 0 ||
            !IsRgb565Format(request.pixel_format))
        {
            plan.result = kDdErrInvalidPixelFormat;
            return plan;
        }
        shape.kind = SurfaceKind::kTexture;
        shape.width = request.width;
        shape.height = request.height;
        shape.caps = caps;
        return plan;
    }
    if ((caps & kDdsCapsOffscreenPlain) != 0)
    {
        constexpr std::uint32_t kRequired = kDdsdCaps | kDdsdWidth | kDdsdHeight;
        if ((request.flags & kRequired) != kRequired || request.width == 0 || request.height == 0 ||
            ((request.flags & kDdsdPixelFormat) != 0 && !IsRgb565Format(request.pixel_format)))
        {
            plan.result = kDdErrInvalidPixelFormat;
            return plan;
        }
        shape.kind = SurfaceKind::kOffscreen;
        shape.width = request.width;
        shape.height = request.height;
        shape.caps = caps;
        return plan;
    }
    if ((caps & kDdsCapsPrimarySurface) == 0 || request.back_buffer_count > 1)
    {
        plan.result = kDdErrUnsupported;
        return plan;
    }
    plan.has_back_buffer = request.back_buffer_count == 1;
    shape.kind = SurfaceKind::kPrimary;
    shape.width = display.mode.width;
    shape.height = display.mode.height;
    shape.bits_per_pixel = display.mode.bits_per_pixel;
    shape.caps = kDdsCapsPrimarySurface | (plan.has_back_buffer ? (kDdsCapsComplex | kDdsCapsFlip) : 0) |
                 (caps & kDdsCaps3dDevice);
    if (plan.has_back_buffer)
    {
        plan.back_buffer = shape;
        plan.back_buffer.kind = SurfaceKind::kBackBuffer;
        plan.back_buffer.caps = kDdsCapsBackBuffer | kDdsCaps3dDevice;
    }
    return plan;
}

bool IsRgb565Format(const DdPixelFormat& format)
{
    return format.size == sizeof(DdPixelFormat) && (format.flags & kDdpfRgb) != 0 && format.bit_count == 16 &&
           format.red_mask == 0xF800U && format.green_mask == 0x07E0U && format.blue_mask == 0x001FU;
}

std::uint32_t Rgb565Pitch(std::uint32_t width)
{
    return (width * 2 + 3) & ~std::uint32_t{3};
}

DdSurfaceDesc2 SurfaceDescription(const SurfaceShape& shape, std::uint32_t pitch)
{
    DdSurfaceDesc2 desc;
    desc.size = sizeof(DdSurfaceDesc2);
    desc.flags = kDdsdCaps | kDdsdHeight | kDdsdWidth | kDdsdPixelFormat | kDdsdPitch;
    desc.height = shape.height;
    desc.width = shape.width;
    desc.pitch = pitch;
    desc.caps.caps = shape.caps;
    desc.pixel_format = Rgb565Format();
    return desc;
}

std::uint32_t CheckAttachment(const SurfaceShape& attachment)
{
    return (attachment.caps & kDdsCapsZBuffer) != 0 ? kDdOk : kDdErrCannotAttachSurface;
}

AttachmentQuery QueryAttachment(const DdsCaps2& caps)
{
    if ((caps.caps & kDdsCapsBackBuffer) != 0)
    {
        return AttachmentQuery::kBackBuffer;
    }
    if ((caps.caps & kDdsCapsZBuffer) != 0)
    {
        return AttachmentQuery::kDepth;
    }
    return AttachmentQuery::kNone;
}

}  // namespace re2dj::directx
