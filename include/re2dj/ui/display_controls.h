#ifndef RE2DJ_UI_DISPLAY_CONTROLS_H_
#define RE2DJ_UI_DISPLAY_CONTROLS_H_

#include "re2dj/ui/osd.h"

namespace re2dj::ui
{

// The OSD's "32-bit color" control: the process's colour-depth selection
// (graphics/color_depth.h), which every host offers the same way. The OSD
// reads it back each frame, so a depth chosen on the command line shows as
// the control's starting state.
void AddColorDepthToggle(Osd* osd);

}  // namespace re2dj::ui

#endif  // RE2DJ_UI_DISPLAY_CONTROLS_H_
