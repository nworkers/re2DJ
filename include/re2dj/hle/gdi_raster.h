#ifndef RE2DJ_HLE_GDI_RASTER_H_
#define RE2DJ_HLE_GDI_RASTER_H_

#include <array>
#include <cstdint>
#include <span>

// GDI's pixel rules as measured on Windows 11, for the Linux facade's
// drawing into guest bitmaps.
namespace re2dj::hle
{

// A pixel layout: its size and each channel's mask (red, green, blue).
struct GdiPixelLayout
{
    std::uint32_t bits_per_pixel = 0;
    std::array<std::uint32_t, 3> masks{};
};

inline constexpr GdiPixelLayout kGdiRgb555 = {16, {0x7C00U, 0x03E0U, 0x001FU}};
inline constexpr GdiPixelLayout kGdiRgb565 = {16, {0xF800U, 0x07E0U, 0x001FU}};
inline constexpr GdiPixelLayout kGdiBgr888 = {24, {0xFF0000U, 0x00FF00U, 0x0000FFU}};
// A COLORREF (0x00BBGGRR) as a pixel: red in the low byte.
inline constexpr GdiPixelLayout kGdiColorref = {24, {0x0000FFU, 0x00FF00U, 0xFF0000U}};

// A channel moved between widths: narrowing drops low bits, widening repeats
// the high bits below them (5-bit green 0x1F becomes 6-bit 0x3F).
std::uint32_t ConvertGdiChannel(std::uint32_t value, std::uint32_t from_bits, std::uint32_t to_bits);

// One pixel read in `from` and written in `to`.
std::uint32_t ConvertGdiPixel(std::uint32_t pixel, const GdiPixelLayout& from, const GdiPixelLayout& to);

// Reads and writes a pixel of the layout's size at the start of bytes.
std::uint32_t ReadGdiPixel(std::span<const std::uint8_t> bytes, std::uint32_t bits_per_pixel);
void WriteGdiPixel(std::span<std::uint8_t> bytes, std::uint32_t bits_per_pixel, std::uint32_t pixel);

// StretchBlt's nearest-neighbour mapping: the source column (or row) offset
// for destination offset `index` of `destination_extent`, stretched from
// `source_extent`.
std::uint32_t StretchSourceIndex(std::uint32_t index, std::uint32_t destination_extent, std::uint32_t source_extent);

// A rectangle as GDI takes one: left and top inside, right and bottom just
// outside.
struct GdiRect
{
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
    bool empty() const { return left >= right || top >= bottom; }
};

// FillRect's area on a bitmap, as measured: the rectangle ordered (an
// inverted one fills the same pixels) and clipped to the bitmap.
GdiRect FillArea(const GdiRect& rect, std::uint32_t width, std::uint32_t height);

// Bytes in a DIB row: whole DWORDs.
std::uint32_t DibRowBytes(std::uint32_t width, std::uint32_t bits_per_pixel);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GDI_RASTER_H_
