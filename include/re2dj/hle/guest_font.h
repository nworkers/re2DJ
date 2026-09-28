#ifndef RE2DJ_HLE_GUEST_FONT_H_
#define RE2DJ_HLE_GUEST_FONT_H_

#include <array>
#include <cstdint>

// The font the Linux facade draws a DC's default font with. Windows draws
// with the Korean "System" bitmap font, 16 pixels high; its glyphs are
// Microsoft's, so GNU Unifont's 8x16 glyphs (SIL OFL 1.1, see
// third_party/unifont) stand in. Unifont is fixed-width where System is not,
// so text of narrow or wide letters ("i", "W") lays out differently.
namespace re2dj::hle
{

inline constexpr std::int32_t kGuestFontHeight = 16;
inline constexpr std::int32_t kGuestFontCharWidth = 8;

// A printable ASCII character's 16 rows, the leftmost pixel in bit 7, or
// null for any other byte.
const std::array<std::uint8_t, 16>* GuestFontGlyph(unsigned char character);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_FONT_H_
