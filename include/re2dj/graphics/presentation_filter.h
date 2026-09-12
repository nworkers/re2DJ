#ifndef RE2DJ_GRAPHICS_PRESENTATION_FILTER_H_
#define RE2DJ_GRAPHICS_PRESENTATION_FILTER_H_

#include <cstdint>

namespace re2dj::graphics
{

enum class PresentationFilter
{
    kNearest,
    kLinear,
};

PresentationFilter SelectPresentationFilter(std::uint32_t logical_width,
                                             std::uint32_t logical_height,
                                             std::uint32_t presentation_width,
                                             std::uint32_t presentation_height);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_PRESENTATION_FILTER_H_
