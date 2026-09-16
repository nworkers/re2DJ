#pragma once

#define NOMINMAX
#include <windows.h>

#include "re2dj/graphics/sdl3_opengl_backend.h"
#include "re2dj/ui/osd.h"

namespace re2dj::platform::windows
{

// The process's single on-screen display, created on first use and kept for
// the life of the process. Its OpenGL objects belong to a context that can be
// gone by the time static destructors run, so it is deliberately never freed.
re2dj::ui::Osd& ProcessOsd();

// Installs the OSD on a backend that has just been initialized, and registers
// the controls this run offers. Safe to call once per backend.
void InstallProcessOsd(re2dj::graphics::Sdl3OpenGlBackend* backend);

// Routes one guest-window message to the OSD. Returns true when the message
// belonged to the OSD and must not reach the guest.
//
// Backtick toggles the display. Swallowing it hides it from the guest's window
// procedure only; a guest that polls key state could still see it, which is
// why the toggle is a key no supported input configuration uses.
bool HandleOsdWindowMessage(UINT message, WPARAM wparam, LPARAM lparam);

// Records OSD state changes to the graphics trace: visibility, the renderer
// coming up or failing, and drawing starting after a show. Called after each
// present; writes only when something changed.
void ReportOsdState();

}  // namespace re2dj::platform::windows
