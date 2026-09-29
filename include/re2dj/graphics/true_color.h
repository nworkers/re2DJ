#ifndef RE2DJ_GRAPHICS_TRUE_COLOR_H_
#define RE2DJ_GRAPHICS_TRUE_COLOR_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "re2dj/graphics/legacy_texture.h"

// The host-side colour a surface keeps next to its guest-visible RGB565
// pixels in 32-bit mode (color_depth.h).
//
// A plane holds one XRGB8888 pixel (0x00RRGGBB) per surface pixel, rows top
// down, and always keeps one invariant: narrowing a plane pixel to 5-6-5 gives
// the surface's RGB565 pixel. The guest therefore sees exactly what it would
// without a plane, while whatever reached the surface at 24 bits keeps them.
namespace re2dj::graphics
{

// Narrowing drops each channel's low bits, as Windows GDI does when it moves a
// colour into a 16-bit bitmap (hle/gdi_raster.h, measured). Widening gives
// c * 255 / 31 (green: / 63), the value the render backend has always
// uploaded a 565 texel as, and narrows back to the same 565 value.
std::uint16_t NarrowToRgb565(std::uint32_t xrgb);
std::uint32_t WidenRgb565(std::uint16_t rgb565);

// Rows `stride` pixels apart, not owned.
struct TrueColorView
{
    std::uint32_t* pixels = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t stride = 0;

    std::uint32_t* Row(std::uint32_t y) const { return pixels + static_cast<std::size_t>(y) * stride; }
};

// A plane in host memory, black to begin with, which is what a surface's
// zeroed RGB565 memory widens to.
class TrueColorPlane
{
public:
    TrueColorPlane(std::uint32_t width, std::uint32_t height);

    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    std::uint32_t* Row(std::uint32_t y) { return pixels_.data() + static_cast<std::size_t>(y) * width_; }
    const std::uint32_t* Row(std::uint32_t y) const
    {
        return pixels_.data() + static_cast<std::size_t>(y) * width_;
    }
    TrueColorView View() { return {pixels_.data(), width_, height_, width_}; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::uint32_t> pixels_;
};

// Row operations. RGB565 rows are little-endian bytes, as guest memory holds
// them, so a host copy of guest memory and a host surface both pass directly.

// The plane values of pixels known only as RGB565.
void WidenRgb565Row(const std::uint8_t* rgb565, std::uint32_t* plane, std::uint32_t count);
// The RGB565 pixels a plane row stands for.
void NarrowTrueColorRow(const std::uint32_t* plane, std::uint8_t* rgb565, std::uint32_t count);
// Matches a plane row to RGB565 pixels something else may have changed: a
// plane pixel that still narrows to its RGB565 pixel stays, keeping its
// precision; any other becomes the widened RGB565 pixel.
void ReconcileTrueColorRow(std::uint32_t* plane, const std::uint8_t* rgb565, std::uint32_t count);
// The plane side of a surface copy. Each source pixel whose RGB565 value the
// key does not match lands in destination: the source plane's value when
// the source has a plane, the widened RGB565 value otherwise. The key is
// decided on RGB565, as the copy of the RGB565 pixels decides it.
void CopyTrueColorRow(std::uint32_t* destination,
                      const std::uint32_t* source_plane,
                      const std::uint8_t* source_rgb565,
                      std::uint32_t count,
                      const Rgb565ColorKey& key);

// The same over rectangles of host surfaces, clipped to nothing: a rectangle
// that does not fit both sides is refused (false) and nothing changes.
bool WidenRgb565Rectangle(const Rgb565SurfaceView& rgb565, const TrueColorView& plane, const Rgb565Rectangle& area);
bool NarrowTrueColorRectangle(const TrueColorView& plane, const Rgb565SurfaceView& rgb565, const Rgb565Rectangle& area);
bool ReconcileTrueColorRectangle(const TrueColorView& plane,
                                 const Rgb565SurfaceView& rgb565,
                                 const Rgb565Rectangle& area);
bool FillTrueColorRectangle(const TrueColorView& plane, const Rgb565Rectangle& area, std::uint32_t xrgb);
// The plane side of CopyRgb565Rectangle, with the same arguments plus the
// planes; source_plane may be null. Safe when both planes are one.
bool CopyTrueColorRectangle(const TrueColorView& destination,
                            std::uint32_t destination_x,
                            std::uint32_t destination_y,
                            const TrueColorView* source_plane,
                            const LegacyTextureView& source,
                            const Rgb565Rectangle& source_rectangle,
                            const Rgb565ColorKey& source_color_key);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_TRUE_COLOR_H_
