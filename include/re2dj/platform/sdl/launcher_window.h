#ifndef RE2DJ_PLATFORM_SDL_LAUNCHER_WINDOW_H_
#define RE2DJ_PLATFORM_SDL_LAUNCHER_WINDOW_H_

#include <string>
#include <vector>

#include "re2dj/launcher/launcher_settings.h"
#include "re2dj/ui/launcher_screen.h"

namespace re2dj::platform::sdl
{

struct LauncherWindowResult
{
    // False when no window could be opened (no display, no OpenGL); `message`
    // says why and the caller falls back to its behaviour without a launcher.
    bool opened = false;
    std::string message;
    // kStart or kQuit; closing the window is kQuit.
    ui::LauncherScreenAction action = ui::LauncherScreenAction::kQuit;
    // The settings as the user left them, the chosen profile in last_profile.
    launcher::LauncherSettings settings;
};

// Opens the launcher window (#12), runs it until the user starts a profile or
// quits, and closes the window and its OpenGL context before returning, so a
// game started next has the GPU to itself. Keyboard, mouse and SDL gamepads
// drive it through Dear ImGui's SDL3 backend.
LauncherWindowResult RunLauncherWindow(const ui::LauncherScreenModel& model,
                                       const launcher::LauncherSettings& initial_settings);

// The screen shader ids a run can choose besides "none", from the same
// directory a run's window lists (`shaders/` in the working directory or
// beside the executable).
std::vector<std::string> ListLauncherPostShaders();

}  // namespace re2dj::platform::sdl

#endif  // RE2DJ_PLATFORM_SDL_LAUNCHER_WINDOW_H_
