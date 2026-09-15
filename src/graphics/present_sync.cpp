#include "re2dj/graphics/present_sync.h"

namespace re2dj::graphics
{

const char* PresentSyncName(PresentSync value)
{
    switch (value)
    {
    case PresentSync::kImmediate:
        return "immediate";
    case PresentSync::kAdaptive:
        return "adaptive";
    case PresentSync::kVerticalSync:
    default:
        return "vsync";
    }
}

bool ParsePresentSyncName(std::string_view text, PresentSync* value)
{
    if (value == nullptr)
    {
        return false;
    }
    if (text == "vsync")
    {
        *value = PresentSync::kVerticalSync;
        return true;
    }
    if (text == "immediate")
    {
        *value = PresentSync::kImmediate;
        return true;
    }
    if (text == "adaptive")
    {
        *value = PresentSync::kAdaptive;
        return true;
    }
    // An unrecognized word is rejected rather than defaulted: it arrives from a
    // command line, where a typo should be reported instead of silently
    // selecting a policy the caller did not ask for.
    return false;
}

}  // namespace re2dj::graphics
