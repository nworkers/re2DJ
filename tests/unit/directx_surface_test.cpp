#include "re2dj/directx/directdraw_surface.h"

#include <cstdint>

#include "re2dj/directx/direct3d_description.h"
#include "test_support.h"

namespace
{

namespace dx = re2dj::directx;

dx::DdSurfaceDesc2 Request(std::uint32_t flags, std::uint32_t caps, std::uint32_t width = 0, std::uint32_t height = 0)
{
    dx::DdSurfaceDesc2 request;
    request.size = sizeof(dx::DdSurfaceDesc2);
    request.flags = flags;
    request.caps.caps = caps;
    request.width = width;
    request.height = height;
    return request;
}

// The surfaces the 4th asks for (graphics-path analysis §2): a flipping 3D
// primary with one back buffer, a 16-bit depth surface, and RGB565 textures.
void CheckFourthSurfaces(re2dj::test::Context& context)
{
    const dx::DirectDrawDisplay display;
    dx::DdSurfaceDesc2 primary = Request(dx::kDdsdCaps | dx::kDdsdBackBufferCount, 0x00002218U);
    primary.back_buffer_count = 1;
    const dx::SurfacePlan primary_plan = dx::PlanCreateSurface(primary, display);
    RE2DJ_CHECK_EQ(context, primary_plan.result, dx::kDdOk);
    RE2DJ_CHECK(context, primary_plan.retains_frames);
    RE2DJ_CHECK(context, primary_plan.has_back_buffer);
    RE2DJ_CHECK(context, primary_plan.surface.kind == dx::SurfaceKind::kPrimary);
    RE2DJ_CHECK_EQ(context, primary_plan.surface.caps,
                   dx::kDdsCapsPrimarySurface | dx::kDdsCapsComplex | dx::kDdsCapsFlip | dx::kDdsCaps3dDevice);
    RE2DJ_CHECK_EQ(context, primary_plan.surface.width, 640U);
    RE2DJ_CHECK(context, primary_plan.back_buffer.kind == dx::SurfaceKind::kBackBuffer);
    RE2DJ_CHECK_EQ(context, primary_plan.back_buffer.caps, dx::kDdsCapsBackBuffer | dx::kDdsCaps3dDevice);

    dx::DdSurfaceDesc2 depth = Request(0x00001007U, 0x00024000U, 640, 480);
    depth.pixel_format = dx::Depth16Format();
    const dx::SurfacePlan depth_plan = dx::PlanCreateSurface(depth, display);
    RE2DJ_CHECK_EQ(context, depth_plan.result, dx::kDdOk);
    RE2DJ_CHECK(context, depth_plan.surface.kind == dx::SurfaceKind::kDepth);
    RE2DJ_CHECK(context, !depth_plan.surface.has_pixels());
    RE2DJ_CHECK_EQ(context, dx::CheckAttachment(depth_plan.surface), dx::kDdOk);

    dx::DdSurfaceDesc2 texture = Request(0x00001007U, 0x10005000U, 1024, 512);
    texture.pixel_format = dx::Rgb565Format();
    const dx::SurfacePlan texture_plan = dx::PlanCreateSurface(texture, display);
    RE2DJ_CHECK_EQ(context, texture_plan.result, dx::kDdOk);
    RE2DJ_CHECK(context, texture_plan.surface.kind == dx::SurfaceKind::kTexture);
    RE2DJ_CHECK_EQ(context, dx::CheckAttachment(texture_plan.surface), dx::kDdErrCannotAttachSurface);
}

// Requests outside the model are refused as the Windows facade refuses them.
void CheckRefusals(re2dj::test::Context& context)
{
    const dx::DirectDrawDisplay display;
    dx::DdSurfaceDesc2 wrong_size = Request(dx::kDdsdCaps, dx::kDdsCapsPrimarySurface);
    wrong_size.size = 108;
    RE2DJ_CHECK_EQ(context, dx::PlanCreateSurface(wrong_size, display).result, dx::kDdErrInvalidParams);

    dx::DdSurfaceDesc2 texture = Request(0x00001007U, dx::kDdsCapsTexture, 64, 64);
    texture.pixel_format = dx::Rgb565Format();
    texture.pixel_format.bit_count = 32;
    RE2DJ_CHECK_EQ(context, dx::PlanCreateSurface(texture, display).result, dx::kDdErrInvalidPixelFormat);

    const dx::DdSurfaceDesc2 offscreen_no_size = Request(dx::kDdsdCaps, dx::kDdsCapsOffscreenPlain);
    RE2DJ_CHECK_EQ(context, dx::PlanCreateSurface(offscreen_no_size, display).result,
                   dx::kDdErrInvalidPixelFormat);
    const dx::DdSurfaceDesc2 offscreen = Request(0x00000007U, dx::kDdsCapsOffscreenPlain, 320, 240);
    RE2DJ_CHECK_EQ(context, dx::PlanCreateSurface(offscreen, display).result, dx::kDdOk);

    dx::DdSurfaceDesc2 two_back_buffers = Request(dx::kDdsdCaps | dx::kDdsdBackBufferCount, 0x00002218U);
    two_back_buffers.back_buffer_count = 2;
    const dx::SurfacePlan refused = dx::PlanCreateSurface(two_back_buffers, display);
    RE2DJ_CHECK_EQ(context, refused.result, dx::kDdErrUnsupported);
    // A flipping primary still tells the facade how the guest presents.
    RE2DJ_CHECK(context, refused.retains_frames);
    RE2DJ_CHECK_EQ(context, dx::PlanCreateSurface(Request(dx::kDdsdCaps, 0), display).result, dx::kDdErrUnsupported);
}

// Descriptions, pitch, and attachment queries.
void CheckDescriptions(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, dx::Rgb565Pitch(640), 1280U);
    RE2DJ_CHECK_EQ(context, dx::Rgb565Pitch(3), 8U);
    dx::SurfaceShape shape;
    shape.width = 640;
    shape.height = 480;
    shape.caps = dx::kDdsCapsBackBuffer;
    const dx::DdSurfaceDesc2 desc = dx::SurfaceDescription(shape, 1280);
    RE2DJ_CHECK_EQ(context, desc.size, 124U);
    RE2DJ_CHECK_EQ(context, desc.pitch, 1280U);
    RE2DJ_CHECK_EQ(context, desc.caps.caps, dx::kDdsCapsBackBuffer);
    RE2DJ_CHECK(context, dx::IsRgb565Format(desc.pixel_format));
    dx::DdsCaps2 caps;
    caps.caps = dx::kDdsCapsBackBuffer;
    RE2DJ_CHECK(context, dx::QueryAttachment(caps) == dx::AttachmentQuery::kBackBuffer);
    caps.caps = dx::kDdsCapsZBuffer;
    RE2DJ_CHECK(context, dx::QueryAttachment(caps) == dx::AttachmentQuery::kDepth);
    caps.caps = dx::kDdsCapsTexture;
    RE2DJ_CHECK(context, dx::QueryAttachment(caps) == dx::AttachmentQuery::kNone);
}

// Lock gives the surface's own pixels: the rectangle's size and first
// pixel, the whole pitch, and the facade's refusals in order.
void CheckLockPlan(re2dj::test::Context& context)
{
    dx::SurfaceShape shape;
    shape.kind = dx::SurfaceKind::kBackBuffer;
    shape.width = 640;
    shape.height = 480;
    shape.caps = dx::kDdsCapsBackBuffer;
    dx::SurfaceLockPlan plan = dx::PlanLock(shape, 1280, true, true, false, nullptr);
    RE2DJ_CHECK_EQ(context, plan.result, dx::kDdOk);
    RE2DJ_CHECK_EQ(context, plan.offset, 0U);
    RE2DJ_CHECK_EQ(context, plan.description.width, 640U);
    RE2DJ_CHECK_EQ(context, plan.description.pitch, 1280U);
    RE2DJ_CHECK_EQ(context, plan.description.flags & dx::kDdsdLpSurface, dx::kDdsdLpSurface);
    const dx::SurfaceRect rect = {10, 20, 30, 60};
    plan = dx::PlanLock(shape, 1280, true, true, false, &rect);
    RE2DJ_CHECK_EQ(context, plan.result, dx::kDdOk);
    RE2DJ_CHECK_EQ(context, plan.offset, 20U * 1280U + 10U * 2U);
    RE2DJ_CHECK_EQ(context, plan.description.width, 20U);
    RE2DJ_CHECK_EQ(context, plan.description.height, 40U);
    RE2DJ_CHECK_EQ(context, plan.description.pitch, 1280U);
    RE2DJ_CHECK_EQ(context, dx::PlanLock(shape, 1280, true, false, false, nullptr).result, dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::PlanLock(shape, 1280, false, true, true, nullptr).result, dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::PlanLock(shape, 1280, false, true, false, nullptr).result, dx::kDdErrInvalidObject);
    for (const dx::SurfaceRect bad : {dx::SurfaceRect{-1, 0, 10, 10}, dx::SurfaceRect{5, 5, 5, 10},
                                      dx::SurfaceRect{0, 0, 641, 10}, dx::SurfaceRect{0, 10, 10, 481}})
    {
        RE2DJ_CHECK_EQ(context, dx::PlanLock(shape, 1280, true, true, false, &bad).result, dx::kDdErrInvalidRect);
    }
}

// EnumSurfaces' flags, as Windows 11 accepts and refuses them.
void CheckEnumSurfacesPlan(re2dj::test::Context& context)
{
    constexpr std::uint32_t kAll = dx::kDdEnumSurfacesAll;
    constexpr std::uint32_t kMatch = dx::kDdEnumSurfacesMatch;
    constexpr std::uint32_t kNoMatch = dx::kDdEnumSurfacesNoMatch;
    constexpr std::uint32_t kExists = dx::kDdEnumSurfacesDoesExist;
    constexpr std::uint32_t kCreatable = dx::kDdEnumSurfacesCanBeCreated;
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll | kExists, false, true) == dx::EnumSurfacesPlan::kExisting);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll | kExists, true, true) == dx::EnumSurfacesPlan::kExisting);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll | kExists, false, false) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kExists, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(0, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kMatch | kExists, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll | kMatch | kExists, true, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll | kCreatable, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context,
                dx::PlanEnumSurfaces(kAll | kExists | kCreatable, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kAll | kExists | 0x100U, false, true) == dx::EnumSurfacesPlan::kInvalid);
    RE2DJ_CHECK(context, dx::PlanEnumSurfaces(kMatch | kExists, true, true) == dx::EnumSurfacesPlan::kUnmodelled);
    RE2DJ_CHECK(context,
                dx::PlanEnumSurfaces(kNoMatch | kCreatable, true, true) == dx::EnumSurfacesPlan::kUnmodelled);
}

}  // namespace

void RunDirectXSurfaceTests(re2dj::test::Context& context)
{
    CheckFourthSurfaces(context);
    CheckRefusals(context);
    CheckDescriptions(context);
    CheckLockPlan(context);
    CheckEnumSurfacesPlan(context);
}
