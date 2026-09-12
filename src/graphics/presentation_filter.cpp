#include "re2dj/graphics/presentation_filter.h"

namespace re2dj::graphics
{

PresentationFilter SelectPresentationFilter(std::uint32_t logical_width,
                                             std::uint32_t logical_height,
                                             std::uint32_t presentation_width,
                                             std::uint32_t presentation_height)
{
    if (logical_width == 0 || logical_height == 0 || presentation_width == 0 ||
        presentation_height == 0)
    {
        return PresentationFilter::kLinear;
    }

    if (presentation_width % logical_width != 0 || presentation_height % logical_height != 0)
    {
        return PresentationFilter::kLinear;
    }

    const std::uint32_t horizontal_scale = presentation_width / logical_width;
    const std::uint32_t vertical_scale = presentation_height / logical_height;
    if (horizontal_scale == 0 || horizontal_scale != vertical_scale)
    {
        return PresentationFilter::kLinear;
    }

    return PresentationFilter::kNearest;
}

}  // namespace re2dj::graphics
