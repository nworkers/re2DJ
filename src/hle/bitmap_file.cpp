#include "re2dj/hle/bitmap_file.h"

#include <algorithm>
#include <utility>

#include "re2dj/hle/gdi_raster.h"

namespace re2dj::hle
{
namespace
{

constexpr std::size_t kFileHeaderSize = 14;
constexpr std::uint32_t kInfoHeaderSize = 40;
constexpr std::uint32_t kBiRgb = 0;

std::uint32_t U32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint16_t U16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

BitmapFileOutcome NotModelled(std::string* reason, std::string text)
{
    if (reason != nullptr)
    {
        *reason = std::move(text);
    }
    return BitmapFileOutcome::kNotModelled;
}

}  // namespace

BitmapFileOutcome ParseBitmapFile(std::span<const std::uint8_t> bytes,
                                  BitmapFileImage* image,
                                  std::string* reason)
{
    if (bytes.size() < 2 || bytes[0] != 'B' || bytes[1] != 'M')
    {
        return BitmapFileOutcome::kNotBitmap;
    }
    if (bytes.size() < kFileHeaderSize + kInfoHeaderSize)
    {
        return NotModelled(reason, "a BMP shorter than its headers");
    }
    const std::uint32_t pixel_offset = U32(bytes, 10);
    const std::uint32_t header_size = U32(bytes, kFileHeaderSize);
    const auto width = static_cast<std::int32_t>(U32(bytes, kFileHeaderSize + 4));
    const auto height = static_cast<std::int32_t>(U32(bytes, kFileHeaderSize + 8));
    const std::uint16_t planes = U16(bytes, kFileHeaderSize + 12);
    const std::uint16_t bit_count = U16(bytes, kFileHeaderSize + 14);
    const std::uint32_t compression = U32(bytes, kFileHeaderSize + 16);
    const std::uint32_t colors_used = U32(bytes, kFileHeaderSize + 32);
    if (header_size != kInfoHeaderSize || planes != 1 || compression != kBiRgb ||
        (bit_count != 24 && bit_count != 8))
    {
        return NotModelled(reason, "a BMP with a " + std::to_string(header_size) + "-byte header, " +
                                       std::to_string(bit_count) + " bits, and compression " +
                                       std::to_string(compression));
    }
    if (width <= 0 || height <= 0 || width > 0x4000 || height > 0x4000)
    {
        return NotModelled(reason, "a BMP of " + std::to_string(width) + "x" + std::to_string(height) +
                                       " (top-down or empty)");
    }

    BitmapFileImage read;
    read.width = static_cast<std::uint32_t>(width);
    read.height = static_cast<std::uint32_t>(height);
    read.bits_per_pixel = bit_count;
    read.row_bytes = DibRowBytes(read.width, bit_count);
    if (bit_count == 8)
    {
        const std::uint32_t entries = colors_used == 0 ? 256 : colors_used;
        const std::size_t table = kFileHeaderSize + kInfoHeaderSize;
        if (entries > 256 || table + entries * 4 > bytes.size())
        {
            return NotModelled(reason, "a BMP color table of " + std::to_string(entries) + " entries");
        }
        read.color_table.resize(entries);
        for (std::uint32_t index = 0; index < entries; ++index)
        {
            // RGBQUAD: blue, green, red, reserved.
            const std::size_t entry = table + index * 4;
            read.color_table[index] = (static_cast<std::uint32_t>(bytes[entry + 2]) << 16) |
                                      (static_cast<std::uint32_t>(bytes[entry + 1]) << 8) | bytes[entry];
        }
    }

    const std::uint32_t pixel_bytes = read.width * bit_count / 8;
    if (pixel_offset > bytes.size() ||
        static_cast<std::uint64_t>(read.row_bytes) * read.height > bytes.size() - pixel_offset)
    {
        return NotModelled(reason, "a truncated BMP");
    }
    read.rows.assign(static_cast<std::size_t>(read.row_bytes) * read.height, 0);
    for (std::uint32_t row = 0; row < read.height; ++row)
    {
        const auto source = bytes.begin() + pixel_offset + static_cast<std::size_t>(row) * read.row_bytes;
        std::copy(source, source + pixel_bytes, read.rows.begin() + static_cast<std::size_t>(row) * read.row_bytes);
    }
    *image = std::move(read);
    return BitmapFileOutcome::kRead;
}

}  // namespace re2dj::hle
