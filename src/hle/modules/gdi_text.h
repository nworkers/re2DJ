#ifndef RE2DJ_HLE_MODULES_GDI_TEXT_H_
#define RE2DJ_HLE_MODULES_GDI_TEXT_H_

// Text drawn into a DC's bitmap with the facade's System font: what user32's
// DrawTextA and gdi32's ExtTextOutA share. Internal to src/hle/modules.

#include <cstdint>
#include <string>
#include <string_view>

#include "re2dj/hle/gdi_raster.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/import_dispatcher.h"

namespace re2dj::hle::modules
{

// Division rounding down, as DrawTextA halves the room around its text.
std::int32_t HalfDown(std::int32_t value);

// Draws one line of text into a DC's 16- or 24-bit bitmap for DrawTextA: the
// cell placed by the DT_ format in rect, painted in the background color
// when OPAQUE, then the glyphs' set pixels in the text color, all clipped to
// rect (to the bitmap alone with DT_NOCLIP). False stops the call.
bool DrawTextPixels(const ImportCall& call,
                    GuestProcess& process,
                    std::uint32_t dc_handle,
                    std::string_view text,
                    const GdiRect& rect,
                    std::uint32_t format,
                    std::string* error);

// ExtTextOutA(hdc, x, y, options, lprect, lpString, c, lpDx) with no options,
// rectangle or spacing: the text's cell at (x, y), as DrawTextA places it
// at the top left of a rectangle with DT_NOCLIP (measured to draw the same
// pixels, transparent and opaque). TRUE with the last error 0 once text is
// drawn; a count of 0 is TRUE with the last error kept; a handle that is no
// DC is FALSE with it kept. Options, a rectangle, spacing, and bytes outside
// printable ASCII stop.
bool ExtTextOutA(const ImportCall& call, ImportReturn* result, std::string* error);

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GDI_TEXT_H_
