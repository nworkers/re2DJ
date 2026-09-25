#ifndef RE2DJ_DIRECTX_DIRECTDRAW_DISPLAY_H_
#define RE2DJ_DIRECTX_DIRECTDRAW_DISPLAY_H_

#include <cstdint>
#include <functional>

#include "re2dj/directx/abi.h"
#include "re2dj/directx/directdraw_description.h"

// The display a DirectDraw object drives: the window it cooperates with and
// the mode it set. Both hosts' facades keep one per DirectDraw object and
// change it only through these rules; what a host does with its own windows
// when a guest takes the display is the host's.
namespace re2dj::directx
{

struct DirectDrawDisplay
{
    // The guest's HWND, 0 before SetCooperativeLevel.
    std::uint32_t window = 0;
    // DDSCL_* as the guest asked; nothing depends on them yet.
    std::uint32_t cooperative_flags = 0;
    DisplayMode mode = kDefaultDisplayMode;
};

// The host's window policy for a guest window taking the display at the
// display's mode; false when the host cannot apply it.
using HostWindowPolicy = std::function<bool(std::uint32_t window, const DisplayMode& mode)>;

// IDirectDraw7::SetCooperativeLevel(hWnd, dwFlags). No window is
// DDERR_INVALIDPARAMS; a host policy that fails is DDERR_GENERIC and leaves the
// display as it was. The flags are recorded, not checked.
std::uint32_t SetCooperativeLevel(DirectDrawDisplay* display,
                                  std::uint32_t window,
                                  std::uint32_t flags,
                                  const HostWindowPolicy& apply_host_policy);

// The one mode the presentation path renders: 640x480 at 16 bits. The
// enumeration lists more, which the guest may inspect but not set.
bool IsSupportedDisplayMode(const DisplayMode& mode);

// IDirectDraw7::SetDisplayMode(width, height, bpp, refresh, flags): a
// supported mode is recorded, any other is DDERR_UNSUPPORTEDMODE. The refresh
// rate and flags are not consulted.
std::uint32_t SetDisplayMode(DirectDrawDisplay* display, const DisplayMode& mode);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECTDRAW_DISPLAY_H_
