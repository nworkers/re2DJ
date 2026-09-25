#include "re2dj/directx/directdraw_description.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace re2dj::directx
{
namespace
{

DdCaps MakeDirectDrawCaps(std::uint32_t caps, std::uint32_t caps2, std::uint32_t surface_caps)
{
    DdCaps result;
    result.size = sizeof(DdCaps);
    result.caps = caps;
    result.caps2 = caps2;
    result.surface_caps.caps = surface_caps;
    return result;
}

constexpr std::array<DisplayMode, 15> kDisplayModes = {{
    {320, 240, 16}, {320, 240, 24}, {320, 240, 32},
    {512, 384, 16}, {512, 384, 24}, {512, 384, 32},
    {640, 480, 16}, {640, 480, 24}, {640, 480, 32},
    {800, 600, 16}, {800, 600, 24}, {800, 600, 32},
    {1024, 768, 16}, {1024, 768, 24}, {1024, 768, 32},
}};

template <std::size_t N>
void CopyText(std::array<char, N>* destination, std::string_view text)
{
    const std::size_t count = std::min(text.size(), N - 1);
    std::copy_n(text.begin(), count, destination->begin());
}

}  // namespace

DdCaps DirectDraw7Caps()
{
    return MakeDirectDrawCaps(kDdCaps3d | kDdCapsBlt | kDdCapsColorKey,
                              kDdCaps2Certified | kDdCaps2NoPageLockRequired | kDdCaps2WideSurfaces |
                                  kDdCaps2CanRenderWindowed,
                              kDdsCapsPrimarySurface | kDdsCapsBackBuffer | kDdsCaps3dDevice |
                                  kDdsCapsVideoMemory);
}

DdCaps DirectDraw4Caps()
{
    return MakeDirectDrawCaps(kDdCaps3d, 0, 0);
}

std::span<const DisplayMode> DisplayModes()
{
    return kDisplayModes;
}

DdSurfaceDesc2 DisplayModeDescription(const DisplayMode& mode)
{
    DdSurfaceDesc2 desc;
    desc.size = sizeof(DdSurfaceDesc2);
    desc.flags = kDdsdWidth | kDdsdHeight | kDdsdPitch | kDdsdPixelFormat | kDdsdRefreshRate;
    desc.width = mode.width;
    desc.height = mode.height;
    desc.refresh_rate = kMonitorFrequency;
    desc.pitch = mode.width * (mode.bits_per_pixel / 8);
    desc.pixel_format.size = sizeof(DdPixelFormat);
    desc.pixel_format.flags = kDdpfRgb;
    desc.pixel_format.bit_count = mode.bits_per_pixel;
    if (mode.bits_per_pixel == 16)
    {
        desc.pixel_format.red_mask = 0x0000F800U;
        desc.pixel_format.green_mask = 0x000007E0U;
        desc.pixel_format.blue_mask = 0x0000001FU;
        return desc;
    }
    if (mode.bits_per_pixel == 32)
    {
        desc.pixel_format.flags |= kDdpfAlphaPixels;
        desc.pixel_format.alpha_mask = 0xFF000000U;
    }
    desc.pixel_format.red_mask = 0x00FF0000U;
    desc.pixel_format.green_mask = 0x0000FF00U;
    desc.pixel_format.blue_mask = 0x000000FFU;
    return desc;
}

DdDeviceIdentifier2 DeviceIdentifier()
{
    DdDeviceIdentifier2 identifier;
    CopyText(&identifier.driver, "re2dj.dll");
    CopyText(&identifier.description, "re2DJ HLE Direct3D 7");
    return identifier;
}

}  // namespace re2dj::directx
