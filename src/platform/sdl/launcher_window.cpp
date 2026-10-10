#include "re2dj/platform/sdl/launcher_window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "re2dj/graphics/post_shader_catalog.h"

namespace re2dj::platform::sdl
{
namespace
{

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 640;
// The same OpenGL 2.1 compatibility context the game window asks for, whose
// GLSL is 1.20, so the launcher opens wherever a game can.
constexpr char kGlslVersion[] = "#version 120";
// ImGui's default font is small for a full-window menu; it grows with the
// window beyond its first size.
constexpr float kFontScale = 1.4f;

using ViewportFunction = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
using ClearColorFunction = void(APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
using ClearFunction = void(APIENTRY*)(GLbitfield);

template <typename Function>
bool LoadGlFunction(const char* name, Function* function)
{
    *function = reinterpret_cast<Function>(SDL_GL_GetProcAddress(name));
    return *function != nullptr;
}

// Everything the window owns, released in reverse order however the run ends.
struct WindowSession
{
    bool video = false;
    SDL_Window* window = nullptr;
    SDL_GLContext gl = nullptr;
    ImGuiContext* imgui = nullptr;
    bool sdl_backend = false;
    bool gl_backend = false;

    WindowSession() = default;
    WindowSession(const WindowSession&) = delete;
    WindowSession& operator=(const WindowSession&) = delete;

    ~WindowSession()
    {
        if (gl_backend)
        {
            ImGui_ImplOpenGL3_Shutdown();
        }
        if (sdl_backend)
        {
            ImGui_ImplSDL3_Shutdown();
        }
        if (imgui != nullptr)
        {
            ImGui::DestroyContext(imgui);
        }
        if (gl != nullptr)
        {
            SDL_GL_DestroyContext(gl);
        }
        if (window != nullptr)
        {
            SDL_DestroyWindow(window);
        }
        if (video)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
        }
    }
};

LauncherWindowResult Unavailable(LauncherWindowResult result, const char* what)
{
    result.opened = false;
    result.message = std::string(what) + ": " + SDL_GetError();
    return result;
}

}  // namespace

LauncherWindowResult RunLauncherWindow(const ui::LauncherScreenModel& model,
                                       const launcher::LauncherSettings& initial_settings)
{
    LauncherWindowResult result;
    result.settings = initial_settings;

    WindowSession session;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return Unavailable(result, "cannot start SDL video");
    }
    session.video = true;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    session.window = SDL_CreateWindow(model.title.c_str(), kWindowWidth, kWindowHeight,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (session.window == nullptr)
    {
        return Unavailable(result, "cannot open the launcher window");
    }
    session.gl = SDL_GL_CreateContext(session.window);
    if (session.gl == nullptr || !SDL_GL_MakeCurrent(session.window, session.gl))
    {
        return Unavailable(result, "cannot create an OpenGL context");
    }
    SDL_GL_SetSwapInterval(1);
    ViewportFunction viewport = nullptr;
    ClearColorFunction clear_color = nullptr;
    ClearFunction clear = nullptr;
    if (!LoadGlFunction("glViewport", &viewport) || !LoadGlFunction("glClearColor", &clear_color) ||
        !LoadGlFunction("glClear", &clear))
    {
        return Unavailable(result, "cannot load OpenGL functions");
    }

    IMGUI_CHECKVERSION();
    session.imgui = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    session.sdl_backend = ImGui_ImplSDL3_InitForOpenGL(session.window, session.gl);
    if (!session.sdl_backend)
    {
        return Unavailable(result, "cannot start ImGui's SDL3 backend");
    }
    session.gl_backend = ImGui_ImplOpenGL3_Init(kGlslVersion);
    if (!session.gl_backend)
    {
        return Unavailable(result, "cannot start ImGui's OpenGL backend");
    }
    result.opened = true;

    ui::LauncherScreenState state;
    state.settings = initial_settings;
    const SDL_WindowID window_id = SDL_GetWindowID(session.window);
    ui::LauncherScreenAction action = ui::LauncherScreenAction::kNone;
    while (action == ui::LauncherScreenAction::kNone)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window_id))
            {
                action = ui::LauncherScreenAction::kQuit;
            }
        }
        if ((SDL_GetWindowFlags(session.window) & SDL_WINDOW_MINIMIZED) != 0)
        {
            SDL_Delay(50);
            continue;
        }

        int width = 0;
        int height = 0;
        SDL_GetWindowSize(session.window, &width, &height);
        ImGui::GetStyle().FontScaleMain =
            kFontScale * std::max(1.0f, static_cast<float>(height) / static_cast<float>(kWindowHeight));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ui::LauncherScreenAction drawn = ui::DrawLauncherScreen(model, &state);
        if (action == ui::LauncherScreenAction::kNone)
        {
            action = drawn;
        }
        ImGui::Render();

        int pixel_width = 0;
        int pixel_height = 0;
        SDL_GetWindowSizeInPixels(session.window, &pixel_width, &pixel_height);
        viewport(0, 0, pixel_width, pixel_height);
        clear_color(0.06f, 0.06f, 0.08f, 1.0f);
        clear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(session.window);
    }

    result.action = action;
    result.settings = state.settings;
    return result;
}

std::vector<std::string> ListLauncherPostShaders()
{
    const char* base = SDL_GetBasePath();
    const std::filesystem::path executable_directory = base != nullptr ? std::filesystem::path(base)
                                                                       : std::filesystem::path();
    std::vector<std::string> ids;
    for (const graphics::PostShaderEntry& entry :
         graphics::ListPostShaders(graphics::ResolvePostShaderDirectory(executable_directory)))
    {
        ids.push_back(entry.id);
    }
    return ids;
}

}  // namespace re2dj::platform::sdl
