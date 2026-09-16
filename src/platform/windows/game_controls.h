#pragma once

#include "re2dj/ui/osd.h"

// Written by the launcher, and only when the running executable is the build a
// profile's control was confirmed in. Zero leaves the control unoffered.
extern "C" __declspec(dllexport) unsigned long g_re2dj_autoplay_flag_address;
// The running target's id and the file name of its executable, NUL-terminated,
// written by the launcher for the OSD's information lines.
extern "C" __declspec(dllexport) char g_re2dj_target_id[32];
extern "C" __declspec(dllexport) char g_re2dj_executable_name[64];

namespace re2dj::platform::windows
{

// Fills `osd` with this run's information and the controls the launcher armed.
void RegisterGameControls(re2dj::ui::Osd* osd);

}  // namespace re2dj::platform::windows
