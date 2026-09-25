#ifndef RE2DJ_DIRECTX_DIRECTDRAW_DESCRIPTION_H_
#define RE2DJ_DIRECTX_DIRECTDRAW_DESCRIPTION_H_

#include <cstdint>
#include <span>

#include "re2dj/directx/abi.h"

// What the DirectDraw HLE says about itself: its caps, display modes, and
// identity. Both hosts' facades answer from here, so a guest sees the same
// driver on Windows and on Linux. Direct3D's side is direct3d_description.h.
namespace re2dj::directx
{

// IDirectDraw7::GetCaps, driver and HEL alike. A DirectX 7 driver reports
// capabilities the DirectX 6 facade does not, and the 4th guest's driver stage
// keeps a device only when the driver publishes DDCAPS2_CANRENDERWINDOWED. The
// rest are properties the facade genuinely has: it vouches for its own
// behavior, its surfaces live in host memory with no page lock, and it does
// not reject surfaces wider than the display.
DdCaps DirectDraw7Caps();
// IDirectDraw4::GetCaps: 3D only.
DdCaps DirectDraw4Caps();

// One display mode.
struct DisplayMode
{
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t bits_per_pixel;
};

// The modes IDirectDraw7::EnumDisplayModes lists: five sizes from 320x240 to
// 1024x768, each at 16, 24, and 32 bits.
std::span<const DisplayMode> DisplayModes();
// A mode's DDSURFACEDESC2, with the standard 5-6-5, 8-8-8, and 8-8-8-8 masks
// and a 60 Hz refresh rate.
DdSurfaceDesc2 DisplayModeDescription(const DisplayMode& mode);
// The mode a DirectDraw object has before SetDisplayMode, and the one mode it
// accepts (see directdraw_display.h): 640x480x16.
inline constexpr DisplayMode kDefaultDisplayMode = {640, 480, 16};

// IDirectDraw7::GetMonitorFrequency, in Hz.
inline constexpr std::uint32_t kMonitorFrequency = 60;
// IDirectDraw7::GetAvailableVidMem, total and free alike. Surfaces live in
// host memory, so the guest is given a fixed budget large enough not to gate
// its allocations.
inline constexpr std::uint32_t kReportedVideoMemory = 128U * 1024U * 1024U;

// IDirectDraw7::GetDeviceIdentifier.
DdDeviceIdentifier2 DeviceIdentifier();

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECTDRAW_DESCRIPTION_H_
