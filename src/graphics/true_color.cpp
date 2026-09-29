#include "re2dj/graphics/true_color.h"

#include <algorithm>
#include <cstring>

namespace re2dj::graphics
{
namespace
{

std::uint16_t ReadRgb565(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

// Whether area lies inside a width x height surface and is not empty.
bool Contains(std::uint32_t width, std::uint32_t height, const Rgb565Rectangle& area)
{
    return area.width != 0 && area.height != 0 && area.x <= width && area.width <= width - area.x &&
           area.y <= height && area.height <= height - area.y;
}

bool Fits(const Rgb565SurfaceView& rgb565, const TrueColorView& plane, const Rgb565Rectangle& area)
{
    return rgb565.pixels != nullptr && plane.pixels != nullptr && rgb565.width == plane.width &&
           rgb565.height == plane.height && rgb565.pitch >= static_cast<std::size_t>(rgb565.width) * 2 &&
           plane.stride >= plane.width && Contains(plane.width, plane.height, area);
}

const std::uint8_t* Rgb565At(const Rgb565SurfaceView& view, std::uint32_t x, std::uint32_t y)
{
    return static_cast<const std::uint8_t*>(view.pixels) + static_cast<std::size_t>(y) * view.pitch +
           static_cast<std::size_t>(x) * 2;
}

}  // namespace

std::uint16_t NarrowToRgb565(std::uint32_t xrgb)
{
    return static_cast<std::uint16_t>((((xrgb >> 19) & 0x1FU) << 11) | (((xrgb >> 10) & 0x3FU) << 5) |
                                      ((xrgb >> 3) & 0x1FU));
}

std::uint32_t WidenRgb565(std::uint16_t rgb565)
{
    const std::uint32_t red = ((rgb565 >> 11) & 0x1FU) * 255U / 31U;
    const std::uint32_t green = ((rgb565 >> 5) & 0x3FU) * 255U / 63U;
    const std::uint32_t blue = (rgb565 & 0x1FU) * 255U / 31U;
    return (red << 16) | (green << 8) | blue;
}

TrueColorPlane::TrueColorPlane(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * height, 0)
{
}

void WidenRgb565Row(const std::uint8_t* rgb565, std::uint32_t* plane, std::uint32_t count)
{
    for (std::uint32_t x = 0; x < count; ++x)
    {
        plane[x] = WidenRgb565(ReadRgb565(rgb565 + static_cast<std::size_t>(x) * 2));
    }
}

void NarrowTrueColorRow(const std::uint32_t* plane, std::uint8_t* rgb565, std::uint32_t count)
{
    for (std::uint32_t x = 0; x < count; ++x)
    {
        const std::uint16_t pixel = NarrowToRgb565(plane[x]);
        rgb565[static_cast<std::size_t>(x) * 2] = static_cast<std::uint8_t>(pixel);
        rgb565[static_cast<std::size_t>(x) * 2 + 1] = static_cast<std::uint8_t>(pixel >> 8);
    }
}

void ReconcileTrueColorRow(std::uint32_t* plane, const std::uint8_t* rgb565, std::uint32_t count)
{
    for (std::uint32_t x = 0; x < count; ++x)
    {
        const std::uint16_t pixel = ReadRgb565(rgb565 + static_cast<std::size_t>(x) * 2);
        if (NarrowToRgb565(plane[x]) != pixel)
        {
            plane[x] = WidenRgb565(pixel);
        }
    }
}

void CopyTrueColorRow(std::uint32_t* destination,
                      const std::uint32_t* source_plane,
                      const std::uint8_t* source_rgb565,
                      std::uint32_t count,
                      const Rgb565ColorKey& key)
{
    for (std::uint32_t x = 0; x < count; ++x)
    {
        const std::uint16_t pixel = ReadRgb565(source_rgb565 + static_cast<std::size_t>(x) * 2);
        if (key.enabled && IsRgb565ColorKeyMatch(pixel, key))
        {
            continue;
        }
        destination[x] = source_plane != nullptr ? source_plane[x] : WidenRgb565(pixel);
    }
}

bool WidenRgb565Rectangle(const Rgb565SurfaceView& rgb565, const TrueColorView& plane, const Rgb565Rectangle& area)
{
    if (!Fits(rgb565, plane, area))
    {
        return false;
    }
    for (std::uint32_t y = area.y; y < area.y + area.height; ++y)
    {
        WidenRgb565Row(Rgb565At(rgb565, area.x, y), plane.Row(y) + area.x, area.width);
    }
    return true;
}

bool NarrowTrueColorRectangle(const TrueColorView& plane, const Rgb565SurfaceView& rgb565, const Rgb565Rectangle& area)
{
    if (!Fits(rgb565, plane, area))
    {
        return false;
    }
    for (std::uint32_t y = area.y; y < area.y + area.height; ++y)
    {
        NarrowTrueColorRow(plane.Row(y) + area.x, const_cast<std::uint8_t*>(Rgb565At(rgb565, area.x, y)),
                           area.width);
    }
    return true;
}

bool ReconcileTrueColorRectangle(const TrueColorView& plane,
                                 const Rgb565SurfaceView& rgb565,
                                 const Rgb565Rectangle& area)
{
    if (!Fits(rgb565, plane, area))
    {
        return false;
    }
    for (std::uint32_t y = area.y; y < area.y + area.height; ++y)
    {
        ReconcileTrueColorRow(plane.Row(y) + area.x, Rgb565At(rgb565, area.x, y), area.width);
    }
    return true;
}

bool FillTrueColorRectangle(const TrueColorView& plane, const Rgb565Rectangle& area, std::uint32_t xrgb)
{
    if (plane.pixels == nullptr || plane.stride < plane.width || !Contains(plane.width, plane.height, area))
    {
        return false;
    }
    for (std::uint32_t y = area.y; y < area.y + area.height; ++y)
    {
        std::fill_n(plane.Row(y) + area.x, area.width, xrgb & 0x00FFFFFFU);
    }
    return true;
}

bool CopyTrueColorRectangle(const TrueColorView& destination,
                            std::uint32_t destination_x,
                            std::uint32_t destination_y,
                            const TrueColorView* source_plane,
                            const LegacyTextureView& source,
                            const Rgb565Rectangle& source_rectangle,
                            const Rgb565ColorKey& source_color_key)
{
    const Rgb565Rectangle destination_area = {destination_x, destination_y, source_rectangle.width,
                                              source_rectangle.height};
    if (destination.pixels == nullptr || destination.stride < destination.width || source.pixels == nullptr ||
        source.pitch < static_cast<std::size_t>(source.width) * 2 ||
        !Contains(destination.width, destination.height, destination_area) ||
        !Contains(source.width, source.height, source_rectangle) ||
        (source_plane != nullptr &&
         (source_plane->pixels == nullptr || source_plane->width != source.width ||
          source_plane->height != source.height || source_plane->stride < source_plane->width)))
    {
        return false;
    }
    // A copy within one plane reads everything before writing anything, as
    // CopyRgb565Rectangle does for one surface.
    std::vector<std::uint32_t> snapshot;
    const bool same_plane = source_plane != nullptr && source_plane->pixels == destination.pixels;
    if (same_plane)
    {
        snapshot.resize(static_cast<std::size_t>(source_rectangle.width) * source_rectangle.height);
        for (std::uint32_t y = 0; y < source_rectangle.height; ++y)
        {
            std::memcpy(snapshot.data() + static_cast<std::size_t>(y) * source_rectangle.width,
                        source_plane->Row(source_rectangle.y + y) + source_rectangle.x,
                        static_cast<std::size_t>(source_rectangle.width) * sizeof(std::uint32_t));
        }
    }
    const auto* const source_bytes = static_cast<const std::uint8_t*>(source.pixels);
    for (std::uint32_t y = 0; y < source_rectangle.height; ++y)
    {
        const std::uint8_t* const source_row = source_bytes +
                                               static_cast<std::size_t>(source_rectangle.y + y) * source.pitch +
                                               static_cast<std::size_t>(source_rectangle.x) * 2;
        const std::uint32_t* plane_row = nullptr;
        if (same_plane)
        {
            plane_row = snapshot.data() + static_cast<std::size_t>(y) * source_rectangle.width;
        }
        else if (source_plane != nullptr)
        {
            plane_row = source_plane->Row(source_rectangle.y + y) + source_rectangle.x;
        }
        CopyTrueColorRow(destination.Row(destination_y + y) + destination_x, plane_row, source_row,
                         source_rectangle.width, source_color_key);
    }
    return true;
}

}  // namespace re2dj::graphics
