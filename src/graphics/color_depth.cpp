#include "re2dj/graphics/color_depth.h"

#include <atomic>

namespace re2dj::graphics
{
namespace
{

std::atomic<ColorDepth> g_selected{ColorDepth::k16};

}  // namespace

const char* ColorDepthName(ColorDepth value)
{
    return value == ColorDepth::k32 ? "32" : "16";
}

bool ParseColorDepthName(std::string_view text, ColorDepth* value)
{
    if (value == nullptr)
    {
        return false;
    }
    if (text == "16")
    {
        *value = ColorDepth::k16;
        return true;
    }
    if (text == "32")
    {
        *value = ColorDepth::k32;
        return true;
    }
    // Rejected rather than defaulted, as ParsePresentSyncName does: a typo on
    // a command line should be reported.
    return false;
}

ColorDepth SelectedColorDepth()
{
    return g_selected.load(std::memory_order_relaxed);
}

void SelectColorDepth(ColorDepth value)
{
    g_selected.store(value, std::memory_order_relaxed);
}

}  // namespace re2dj::graphics
