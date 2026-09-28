#include "re2dj/hle/gdi_raster.h"

#include <algorithm>
#include <bit>

namespace re2dj::hle
{

std::uint32_t ConvertGdiChannel(std::uint32_t value, std::uint32_t from_bits, std::uint32_t to_bits)
{
    if (from_bits == 0 || to_bits == 0)
    {
        return 0;
    }
    if (to_bits <= from_bits)
    {
        return value >> (from_bits - to_bits);
    }
    std::uint32_t result = 0;
    std::uint32_t filled = 0;
    while (filled < to_bits)
    {
        const std::uint32_t take = std::min(from_bits, to_bits - filled);
        result = (result << take) | (value >> (from_bits - take));
        filled += take;
    }
    return result;
}

std::uint32_t ConvertGdiPixel(std::uint32_t pixel, const GdiPixelLayout& from, const GdiPixelLayout& to)
{
    std::uint32_t result = 0;
    for (std::size_t channel = 0; channel < 3; ++channel)
    {
        const std::uint32_t from_mask = from.masks[channel];
        const std::uint32_t to_mask = to.masks[channel];
        if (from_mask == 0 || to_mask == 0)
        {
            continue;
        }
        const auto from_shift = static_cast<std::uint32_t>(std::countr_zero(from_mask));
        const auto from_bits = static_cast<std::uint32_t>(std::popcount(from_mask));
        const auto to_shift = static_cast<std::uint32_t>(std::countr_zero(to_mask));
        const auto to_bits = static_cast<std::uint32_t>(std::popcount(to_mask));
        const std::uint32_t value = (pixel & from_mask) >> from_shift;
        result |= (ConvertGdiChannel(value, from_bits, to_bits) << to_shift) & to_mask;
    }
    return result;
}

std::uint32_t ReadGdiPixel(std::span<const std::uint8_t> bytes, std::uint32_t bits_per_pixel)
{
    std::uint32_t pixel = 0;
    for (std::uint32_t index = 0; index < bits_per_pixel / 8; ++index)
    {
        pixel |= static_cast<std::uint32_t>(bytes[index]) << (8 * index);
    }
    return pixel;
}

void WriteGdiPixel(std::span<std::uint8_t> bytes, std::uint32_t bits_per_pixel, std::uint32_t pixel)
{
    for (std::uint32_t index = 0; index < bits_per_pixel / 8; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(pixel >> (8 * index));
    }
}

std::uint32_t StretchSourceIndex(std::uint32_t index, std::uint32_t destination_extent, std::uint32_t source_extent)
{
    return destination_extent == 0
               ? 0
               : static_cast<std::uint32_t>(static_cast<std::uint64_t>(index) * source_extent / destination_extent);
}

GdiRect FillArea(const GdiRect& rect, std::uint32_t width, std::uint32_t height)
{
    GdiRect area;
    area.left = std::max(std::min(rect.left, rect.right), 0);
    area.top = std::max(std::min(rect.top, rect.bottom), 0);
    area.right = std::min(std::max(rect.left, rect.right), static_cast<std::int32_t>(width));
    area.bottom = std::min(std::max(rect.top, rect.bottom), static_cast<std::int32_t>(height));
    return area;
}

std::uint32_t DibRowBytes(std::uint32_t width, std::uint32_t bits_per_pixel)
{
    return ((width * bits_per_pixel + 31U) / 32U) * 4U;
}

}  // namespace re2dj::hle
