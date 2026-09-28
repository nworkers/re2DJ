#ifndef RE2DJ_HLE_BITMAP_FILE_H_
#define RE2DJ_HLE_BITMAP_FILE_H_

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// BMP files as LoadImageA reads them into a DIB section, for the formats the
// EZ2DJ assets use. See docs/design/20260928-421-bitmap-files.md.
namespace re2dj::hle
{

// A BMP file's pixels as a DIB: rows bottom-up, each DWORD-aligned with its
// padding zeroed (Windows 11 does not copy the file's padding bytes).
struct BitmapFileImage
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bits_per_pixel = 0;
    std::uint32_t row_bytes = 0;
    // 8-bit only: the palette as 0x00RRGGBB, biClrUsed entries or 256.
    std::vector<std::uint32_t> color_table;
    std::vector<std::uint8_t> rows;
};

enum class BitmapFileOutcome
{
    kRead,
    // The bytes do not start with "BM"; LoadImageA gives NULL with 0.
    kNotBitmap,
    // A BMP this model does not read: anything but a bottom-up BI_RGB
    // BITMAPINFOHEADER image of 8 or 24 bits, or a truncated one.
    kNotModelled,
};

// Reads a BMP file. For kNotModelled, reason says what was found.
BitmapFileOutcome ParseBitmapFile(std::span<const std::uint8_t> bytes,
                                  BitmapFileImage* image,
                                  std::string* reason);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_BITMAP_FILE_H_
