#ifndef RE2DJ_UI_LAUNCHER_SCREEN_H_
#define RE2DJ_UI_LAUNCHER_SCREEN_H_

#include <string>
#include <vector>

#include "re2dj/launcher/launcher_catalog.h"
#include "re2dj/launcher/launcher_settings.h"

namespace re2dj::ui
{

// What the launcher screen (#12) shows. It draws with Dear ImGui into whatever
// frame the caller has begun and knows nothing of the window, so the SDL host
// owns the window and the event loop.
struct LauncherScreenModel
{
    std::string title;
    std::vector<launcher::LauncherEntry> catalog;
    // Shader ids offered besides "none", in list order.
    std::vector<std::string> post_shaders;
    // The shader a run starts with when none is chosen (RE2DJ_POST_SHADER or
    // "none").
    std::string default_post_shader;
    // How the previous run ended, or empty.
    std::string status;
};

// What the user has chosen so far, kept across frames.
struct LauncherScreenState
{
    // Index into the catalog, or -1 before the first frame picks one.
    int selected = -1;
    launcher::LauncherSettings settings;
    bool focus_placed = false;
};

enum class LauncherScreenAction
{
    kNone,
    kStart,
    kQuit,
};

// Draws one frame of the launcher over the whole display. kStart is returned
// only for a runnable profile; the chosen id is then in
// state->settings.last_profile.
LauncherScreenAction DrawLauncherScreen(const LauncherScreenModel& model, LauncherScreenState* state);

}  // namespace re2dj::ui

#endif  // RE2DJ_UI_LAUNCHER_SCREEN_H_
