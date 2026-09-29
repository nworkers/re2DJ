#ifndef RE2DJ_GRAPHICS_COLOR_DEPTH_H_
#define RE2DJ_GRAPHICS_COLOR_DEPTH_H_

#include <string_view>

namespace re2dj::graphics
{

// How deep the host keeps the colours of what the guest draws. The guest's own
// view never changes: it asked for a 16-bit display and keeps seeing RGB565
// surfaces either way (see true_color.h).
//
// Its own header for the reason present_sync.h gives: a target profile, the
// command line, and the OSD name it without needing the backend.
enum class ColorDepth
{
    // RGB565 throughout, as the original's 16-bit display showed it.
    k16,
    // Surfaces carry a host-side XRGB8888 plane next to their RGB565 pixels,
    // and the render target is 8 bits per channel, so 24-bit sources and
    // blends are not cut down to 565 on the way to the screen.
    k32,
};

// "16" or "32", and the inverse. These are the words the product and the
// launcher options carry.
const char* ColorDepthName(ColorDepth value);
bool ParseColorDepthName(std::string_view text, ColorDepth* value);

// The process's selection. There is one display per process, so one switch:
// the command line sets it before the guest runs and the OSD flips it while it
// does. The ddraw facades and the render backend read it on every operation,
// so a change applies from the next one. Safe from any thread.
ColorDepth SelectedColorDepth();
void SelectColorDepth(ColorDepth value);
inline bool TrueColorSelected()
{
    return SelectedColorDepth() == ColorDepth::k32;
}

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_COLOR_DEPTH_H_
