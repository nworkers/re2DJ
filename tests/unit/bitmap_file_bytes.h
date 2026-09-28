#ifndef RE2DJ_TESTS_UNIT_BITMAP_FILE_BYTES_H_
#define RE2DJ_TESTS_UNIT_BITMAP_FILE_BYTES_H_

// The BMP files the task 421 Windows probe loaded, byte for byte.

#include <cstdint>
#include <vector>

namespace re2dj::test
{

inline void PutLe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

// File and info headers for a bottom-up BI_RGB image.
inline std::vector<std::uint8_t> BitmapHeaders(std::int32_t width,
                                               std::int32_t height,
                                               std::uint16_t bits,
                                               std::uint32_t colors_used,
                                               std::uint32_t table_entries,
                                               std::uint32_t pixel_bytes)
{
    std::vector<std::uint8_t> bytes(54, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    const std::uint32_t offset = 54 + table_entries * 4;
    PutLe32(bytes, 2, offset + pixel_bytes);
    PutLe32(bytes, 10, offset);
    PutLe32(bytes, 14, 40);
    PutLe32(bytes, 18, static_cast<std::uint32_t>(width));
    PutLe32(bytes, 22, static_cast<std::uint32_t>(height));
    bytes[26] = 1;
    bytes[28] = static_cast<std::uint8_t>(bits);
    PutLe32(bytes, 46, colors_used);
    return bytes;
}

// 3x2, 24 bits. Screen top row (r,g,b): (10,20,30) (40,50,60) (70,80,90);
// bottom row: (255,0,0) (0,255,0) (0,0,255). The first row's padding holds
// AA BB CC.
inline std::vector<std::uint8_t> Bitmap24File()
{
    std::vector<std::uint8_t> bytes = BitmapHeaders(3, 2, 24, 0, 0, 24);
    const std::uint8_t pixels[24] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0xAA, 0xBB, 0xCC,
                                     0x1E, 0x14, 0x0A, 0x3C, 0x32, 0x28, 0x5A, 0x50, 0x46, 0x00, 0x00, 0x00};
    bytes.insert(bytes.end(), pixels, pixels + 24);
    return bytes;
}

// 2x2, 8 bits, entry i = (r i, g 255 - i, b 3i mod 256). Memory rows: 1 2,
// then 200 255 (the screen's top row).
inline std::vector<std::uint8_t> Bitmap8File(std::uint32_t colors_used = 0)
{
    const std::uint32_t entries = colors_used == 0 ? 256 : colors_used;
    std::vector<std::uint8_t> bytes = BitmapHeaders(2, 2, 8, colors_used, entries, 8);
    for (std::uint32_t index = 0; index < entries; ++index)
    {
        bytes.push_back(static_cast<std::uint8_t>(index * 3));
        bytes.push_back(static_cast<std::uint8_t>(255 - index));
        bytes.push_back(static_cast<std::uint8_t>(index));
        bytes.push_back(0);
    }
    const std::uint8_t pixels[8] = {1, 2, 0, 0, 200, 255, 0, 0};
    bytes.insert(bytes.end(), pixels, pixels + 8);
    return bytes;
}

}  // namespace re2dj::test

#endif  // RE2DJ_TESTS_UNIT_BITMAP_FILE_BYTES_H_
