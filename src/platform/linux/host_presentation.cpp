#include "re2dj/platform/linux/host_presentation.h"

#include <cmath>
#include <optional>

#include <SDL3/SDL.h>
#include <spdlog/logger.h>

#include "host_keyboard.h"
#include "re2dj/graphics/sdl3_opengl_backend.h"
#include "re2dj/input/virtual_keys.h"
#include "re2dj/logging/logging.h"
#include "re2dj/version.h"

namespace re2dj::platform::linux
{
namespace
{

namespace input = re2dj::input;

// SDL's left, middle, right buttons as the guest's left, right, middle, with
// their virtual keys.
void SetMouseButton(hle::HostInputState* state, std::uint8_t button, bool down)
{
    int index = -1;
    int virtual_key = 0;
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        index = 0;
        virtual_key = input::kVkLButton;
        break;
    case SDL_BUTTON_RIGHT:
        index = 1;
        virtual_key = input::kVkRButton;
        break;
    case SDL_BUTTON_MIDDLE:
        index = 2;
        virtual_key = input::kVkMButton;
        break;
    default:
        return;
    }
    state->mouse_buttons[static_cast<std::size_t>(index)] = down;
    state->virtual_keys.set(static_cast<std::size_t>(virtual_key), down);
}

}  // namespace


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
    // The render target has the display's own size; the window around it is
    // sized by the window policy once it is open.
    config.width = width;
    config.height = height;
    config.title = title.c_str();
    config.resizable = true;
    config.centered = true;
    // Nothing is drawn yet, so the first frame is the guest's cleared screen.
    if (!backend->Initialize(config, error))
    {
        return false;
    }
    backend_ = std::move(backend);
    guest_window_ = guest_window;
    logical_width_ = width;
    logical_height_ = height;
    backend_->SetEventObserver([this](const void* sdl_event) { HandleEvent(sdl_event); });
    if (!ApplyWindowMode(error) || !backend_->ClearRenderTarget(0, error) || !backend_->Present(error))
    {
        backend_.reset();
        return false;
    }
    error->clear();
    return true;
}

bool LinuxHostPresentation::DesktopDisplayMode(hle::HostDisplayMode* mode, std::string* error) const
{
    return graphics::Sdl3OpenGlBackend::QueryDesktopDisplayMode(
        &mode->width, &mode->height, &mode->bits_per_pixel, &mode->refresh_hz, error);
}

bool LinuxHostPresentation::ApplyWindowMode(std::string* error)
{
    if (fullscreen_)
    {
        return backend_->SetFullscreen(true, error);
    }
    return backend_->SetFullscreen(false, error) &&
           backend_->ResizeWindow(logical_width_ * scale_, logical_height_ * scale_, error);
}

void LinuxHostPresentation::ChangeWindowMode(std::uint32_t scale, bool fullscreen)
{
    const std::uint32_t previous_scale = scale_;
    const bool previous_fullscreen = fullscreen_;
    scale_ = scale;
    fullscreen_ = fullscreen;
    std::string error;
    if (!ApplyWindowMode(&error))
    {
        // As the Windows host does, a mode the window cannot take is undone.
        scale_ = previous_scale;
        fullscreen_ = previous_fullscreen;
        ApplyWindowMode(&error);
    }
}

void LinuxHostPresentation::HandleEvent(const void* sdl_event)
{
    const auto* event = static_cast<const SDL_Event*>(sdl_event);
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        close_requested_ = true;
        break;
    case SDL_EVENT_KEY_UP:
        SetHostKey(&input_, event->key.scancode, false);
        break;
    case SDL_EVENT_KEY_DOWN:
    {
        // Held for the guest whatever else the key does, as Windows'
        // GetAsyncKeyState also sees the keys of a window shortcut.
        SetHostKey(&input_, event->key.scancode, true);
        if (event->key.repeat || (event->key.mod & SDL_KMOD_ALT) == 0)
        {
            break;
        }
        std::uint32_t scale = 0;
        switch (event->key.key)
        {
        case SDLK_1:
        case SDLK_KP_1:
            scale = 1;
            break;
        case SDLK_2:
        case SDLK_KP_2:
            scale = 2;
            break;
        case SDLK_3:
        case SDLK_KP_3:
            scale = 3;
            break;
        default:
            break;
        }
        if (graphics::IsWindowScale(scale))
        {
            // In fullscreen the scale waits until the window returns.
            if (fullscreen_)
            {
                scale_ = scale;
            }
            else
            {
                ChangeWindowMode(scale, false);
            }
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        SetMouseButton(&input_, event->button.button, true);
        // Every second click of a run is a double click, as Windows reports
        // WM_LBUTTONDBLCLK.
        if (event->button.button == SDL_BUTTON_LEFT && event->button.clicks >= 2 && event->button.clicks % 2 == 0)
        {
            ChangeWindowMode(scale_, !fullscreen_);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        SetMouseButton(&input_, event->button.button, false);
        break;
    case SDL_EVENT_MOUSE_MOTION:
    {
        int window_width = 0;
        int window_height = 0;
        SDL_Window* window = SDL_GetWindowFromEvent(event);
        if (window != nullptr && SDL_GetWindowSize(window, &window_width, &window_height))
        {
            // The pointer over the drawn display, in the display's own units;
            // over the bars it lies outside it, as over a window's frame.
            const graphics::PresentRect fit =
                graphics::FitPresentation(window_width, window_height, logical_width_, logical_height_);
            if (fit.width > 0 && fit.height > 0)
            {
                input_.cursor_window = guest_window_;
                input_.cursor_x = static_cast<std::int32_t>(
                    std::floor((event->motion.x - static_cast<float>(fit.x)) * static_cast<float>(logical_width_) /
                               static_cast<float>(fit.width)));
                input_.cursor_y = static_cast<std::int32_t>(
                    std::floor((event->motion.y - static_cast<float>(fit.y)) * static_cast<float>(logical_height_) /
                               static_cast<float>(fit.height)));
            }
        }
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    {
        const std::uint32_t window = input_.cursor_window;
        const std::int32_t x = input_.cursor_x;
        const std::int32_t y = input_.cursor_y;
        input_ = {};
        input_.cursor_window = window;
        input_.cursor_x = x;
        input_.cursor_y = y;
        break;
    }
    default:
        break;
    }
}

void LinuxHostPresentation::SetRetainBetweenFrames(bool retain)
{
    if (backend_ != nullptr)
    {
        backend_->SetRetainBetweenFrames(retain);
    }
}

bool LinuxHostPresentation::ClearTarget(std::uint16_t rgb565, std::string* error)
{
    if (backend_ == nullptr)
    {
        *error = "no host window to clear";
        return false;
    }
    return backend_->ClearRenderTarget(rgb565, error);
}

bool LinuxHostPresentation::ReadTarget(std::uint32_t x,
                                       std::uint32_t y,
                                       std::uint32_t width,
                                       std::uint32_t height,
                                       std::span<std::uint8_t> pixels,
                                       std::uint32_t pitch,
                                       std::string* error)
{
    if (backend_ == nullptr)
    {
        *error = "no host window to read";
        return false;
    }
    return backend_->ReadRenderTarget(x, y, width, height, pixels, pitch, error);
}

bool LinuxHostPresentation::WriteTarget(std::uint32_t x,
                                        std::uint32_t y,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        std::span<const std::uint8_t> pixels,
                                        std::uint32_t pitch,
                                        std::string* error)
{
    if (backend_ == nullptr)
    {
        *error = "no host window to write";
        return false;
    }
    return backend_->WriteRenderTarget(x, y, width, height, pixels, pitch, error);
}

bool LinuxHostPresentation::Draw(const graphics::LegacyDrawCommand& command,
                                 const graphics::LegacyFixedFunctionState& state,
                                 std::uint32_t logical_width,
                                 std::uint32_t logical_height,
                                 const graphics::LegacyTextureView* texture,
                                 std::string* error)
{
    if (backend_ == nullptr)
    {
        *error = "no host window to draw into";
        return false;
    }
    return backend_->Draw(command, state, logical_width, logical_height, texture, error);
}

bool LinuxHostPresentation::Present(std::string* error)
{
    if (backend_ == nullptr)
    {
        *error = "no host window to present";
        return false;
    }
    if (!backend_->Present(error))
    {
        return false;
    }
    if (!pacing_reported_ && backend_->software_pacing_engaged())
    {
        pacing_reported_ = true;
        const std::shared_ptr<spdlog::logger> logger = logging::GetLogger();
        if (logger != nullptr)
        {
            logger->info("presentation: the swap does not block, so presents are paced at the display rate");
        }
    }
    const std::optional<double> rate = frame_rate_.Present(SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency());
    if (rate.has_value())
    {
        backend_->SetTitle(WindowTitle(VersionString(), *rate).c_str());
    }
    return true;
}

void LinuxHostPresentation::DiscardTexture(std::uint64_t identity)
{
    if (backend_ != nullptr)
    {
        backend_->DiscardTexture(identity);
    }
}

void LinuxHostPresentation::HoldUntilClosed()
{
    if (backend_ == nullptr)
    {
        return;
    }
    // Waits on the events directly, as no Present runs any more to hand
    // them to the observer.
    SDL_Event event = {};
    while (!close_requested_)
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
