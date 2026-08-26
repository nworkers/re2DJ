#ifndef RE2DJ_GRAPHICS_LEGACY_TEXTURE_H_
#define RE2DJ_GRAPHICS_LEGACY_TEXTURE_H_

#include <cstddef>
#include <cstdint>

namespace re2dj::graphics
{

struct Rgb565ColorKey
{
    bool enabled = false;
    std::uint16_t low = 0;
    std::uint16_t high = 0;
};

struct LegacyTextureView
{
    const void* pixels = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t pitch = 0;
    std::uint64_t identity = 0;
    std::uint64_t revision = 0;
    Rgb565ColorKey source_color_key;
};

struct Rgb565SurfaceView
{
    void* pixels = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t pitch = 0;
};

struct Rgb565Rectangle
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

bool IsRgb565ColorKeyMatch(std::uint16_t pixel, const Rgb565ColorKey& color_key);
bool CopyRgb565Rectangle(const Rgb565SurfaceView& destination,
                         std::uint32_t destination_x,
                         std::uint32_t destination_y,
                         const LegacyTextureView& source,
                         const Rgb565Rectangle& source_rectangle,
                         const Rgb565ColorKey& source_color_key);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_LEGACY_TEXTURE_H_
