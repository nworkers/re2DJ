#include "re2dj/hle/guest_font.h"

namespace re2dj::hle
{
namespace
{

// U+0020 through U+007E.
constexpr std::array<std::uint8_t, 16> kGlyphs[] = {
#include "unifont_ascii_glyphs.inc"
};
static_assert(sizeof(kGlyphs) / sizeof(kGlyphs[0]) == 0x7F - 0x20);

}  // namespace

const std::array<std::uint8_t, 16>* GuestFontGlyph(unsigned char character)
{
    if (character < 0x20 || character > 0x7E)
    {
        return nullptr;
    }
    return &kGlyphs[character - 0x20];
}

}  // namespace re2dj::hle
