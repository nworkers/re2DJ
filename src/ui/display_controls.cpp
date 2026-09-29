#include "re2dj/ui/display_controls.h"

#include "re2dj/graphics/color_depth.h"

namespace re2dj::ui
{
namespace
{

bool ReadTrueColor(void*, bool* value)
{
    *value = graphics::TrueColorSelected();
    return true;
}

bool WriteTrueColor(void*, bool value)
{
    graphics::SelectColorDepth(value ? graphics::ColorDepth::k32 : graphics::ColorDepth::k16);
    return true;
}

}  // namespace

void AddColorDepthToggle(Osd* osd)
{
    if (osd == nullptr)
    {
        return;
    }
    OsdToggle toggle;
    toggle.label = "32-bit color";
    toggle.read = &ReadTrueColor;
    toggle.write = &WriteTrueColor;
    osd->AddToggle(toggle);
}

}  // namespace re2dj::ui
