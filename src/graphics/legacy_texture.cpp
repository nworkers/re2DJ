#include "re2dj/graphics/legacy_texture.h"

#include <cstring>
#include <vector>

namespace re2dj::graphics
{

bool IsRgb565ColorKeyMatch(std::uint16_t pixel, const Rgb565ColorKey& color_key)
{
    return color_key.enabled && color_key.low <= color_key.high &&
           pixel >= color_key.low && pixel <= color_key.high;
}

bool CopyRgb565Rectangle(const Rgb565SurfaceView& destination,
                         std::uint32_t destination_x,
                         std::uint32_t destination_y,
                         const LegacyTextureView& source,
                         const Rgb565Rectangle& source_rectangle,
                         const Rgb565ColorKey& source_color_key)
{
    constexpr std::size_t kPixelSize = sizeof(std::uint16_t);
    if (destination.pixels == nullptr || source.pixels == nullptr ||
        source_rectangle.width == 0 || source_rectangle.height == 0 ||
        destination.pitch < static_cast<std::size_t>(destination.width) * kPixelSize ||
        source.pitch < static_cast<std::size_t>(source.width) * kPixelSize ||
        destination_x > destination.width || destination_y > destination.height ||
        source_rectangle.x > source.width || source_rectangle.y > source.height ||
        source_rectangle.width > destination.width - destination_x ||
        source_rectangle.height > destination.height - destination_y ||
        source_rectangle.width > source.width - source_rectangle.x ||
        source_rectangle.height > source.height - source_rectangle.y)
    {
        return false;
    }

    const auto* source_bytes = static_cast<const std::uint8_t*>(source.pixels);
    auto* destination_bytes = static_cast<std::uint8_t*>(destination.pixels);
    std::vector<std::uint16_t> snapshot;
    if (destination.pixels == source.pixels)
    {
        snapshot.resize(static_cast<std::size_t>(source_rectangle.width) *
                        source_rectangle.height);
        for (std::uint32_t y = 0; y < source_rectangle.height; ++y)
        {
            const auto* row = reinterpret_cast<const std::uint16_t*>(
                source_bytes + static_cast<std::size_t>(source_rectangle.y + y) *
                                   source.pitch);
            std::memcpy(snapshot.data() + static_cast<std::size_t>(y) *
                                              source_rectangle.width,
                        row + source_rectangle.x,
                        static_cast<std::size_t>(source_rectangle.width) * kPixelSize);
        }
    }

    for (std::uint32_t y = 0; y < source_rectangle.height; ++y)
    {
        const auto* source_row = snapshot.empty()
                                     ? reinterpret_cast<const std::uint16_t*>(
                                           source_bytes +
                                           static_cast<std::size_t>(source_rectangle.y + y) *
                                               source.pitch) +
                                           source_rectangle.x
                                     : snapshot.data() +
                                           static_cast<std::size_t>(y) * source_rectangle.width;
        auto* destination_row = reinterpret_cast<std::uint16_t*>(
                                    destination_bytes +
                                    static_cast<std::size_t>(destination_y + y) *
                                        destination.pitch) +
                                destination_x;
        if (!source_color_key.enabled)
        {
            std::memcpy(destination_row,
                        source_row,
                        static_cast<std::size_t>(source_rectangle.width) * kPixelSize);
            continue;
        }
        for (std::uint32_t x = 0; x < source_rectangle.width; ++x)
        {
            if (!IsRgb565ColorKeyMatch(source_row[x], source_color_key))
            {
                destination_row[x] = source_row[x];
            }
        }
    }
    return true;
}

}  // namespace re2dj::graphics
