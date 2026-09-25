#include "re2dj/directx/directdraw_display.h"

namespace re2dj::directx
{

std::uint32_t SetCooperativeLevel(DirectDrawDisplay* display,
                                  std::uint32_t window,
                                  std::uint32_t flags,
                                  const HostWindowPolicy& apply_host_policy)
{
    if (display == nullptr || window == 0)
    {
        return kDdErrInvalidParams;
    }
    if (apply_host_policy && !apply_host_policy(window, display->mode))
    {
        return kDdErrGeneric;
    }
    display->window = window;
    display->cooperative_flags = flags;
    return kDdOk;
}

bool IsSupportedDisplayMode(const DisplayMode& mode)
{
    return mode.width == kDefaultDisplayMode.width && mode.height == kDefaultDisplayMode.height &&
           mode.bits_per_pixel == kDefaultDisplayMode.bits_per_pixel;
}

std::uint32_t SetDisplayMode(DirectDrawDisplay* display, const DisplayMode& mode)
{
    if (display == nullptr)
    {
        return kDdErrInvalidParams;
    }
    if (!IsSupportedDisplayMode(mode))
    {
        return kDdErrUnsupportedMode;
    }
    display->mode = mode;
    return kDdOk;
}

}  // namespace re2dj::directx
