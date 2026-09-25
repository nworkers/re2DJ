#include "re2dj/platform/linux/host_presentation.h"

#include <SDL3/SDL.h>

#include "re2dj/graphics/sdl3_opengl_backend.h"
#include "re2dj/version.h"

namespace re2dj::platform::linux
{

LinuxHostPresentation::LinuxHostPresentation() = default;

LinuxHostPresentation::~LinuxHostPresentation() = default;

bool LinuxHostPresentation::ShowGuestWindow(std::uint32_t guest_window,
                                            std::uint32_t width,
                                            std::uint32_t height,
                                            std::string* error)
{
    if (backend_ != nullptr)
    {
        // One host window stands for the guest's; a second window or size
        // is not modelled yet.
        if (guest_window != guest_window_)
        {
            *error = "the host presents one guest window";
            return false;
        }
        return true;
    }
    auto backend = std::make_unique<graphics::Sdl3OpenGlBackend>();
    const std::string title = WindowTitle(VersionString(), 0.0);
    graphics::Sdl3OpenGlWindowConfig config;
    config.width = width;
    config.height = height;
    config.title = title.c_str();
    // Nothing is drawn yet, so the first frame is the guest's cleared screen.
    if (!backend->Initialize(config, error) || !backend->ClearRenderTarget(0, error) || !backend->Present(error))
    {
        return false;
    }
    backend_ = std::move(backend);
    guest_window_ = guest_window;
    error->clear();
    return true;
}

void LinuxHostPresentation::HoldUntilClosed()
{
    if (backend_ == nullptr)
    {
        return;
    }
    // Waits on the events directly: Present drains the queue, which would
    // swallow the close request.
    SDL_Event event = {};
    while (true)
    {
        if (!SDL_WaitEventTimeout(&event, 250))
        {
            continue;
        }
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            return;
        }
    }
}

}  // namespace re2dj::platform::linux
