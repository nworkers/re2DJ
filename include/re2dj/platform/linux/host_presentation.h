#ifndef RE2DJ_PLATFORM_LINUX_HOST_PRESENTATION_H_
#define RE2DJ_PLATFORM_LINUX_HOST_PRESENTATION_H_

#include <cstdint>
#include <memory>
#include <string>

#include "re2dj/graphics/window_policy.h"
#include "re2dj/hle/host_presentation.h"

namespace re2dj::graphics
{
class Sdl3OpenGlBackend;
}

namespace re2dj::platform::linux
{

// The Linux host's presentation: one SDL3/OpenGL window, made the first time
// the guest takes the display, titled like the Windows product's window, that
// the guest's frames are drawn into through the shared render backend. The
// window follows the shared window policy (graphics/window_policy.h): twice
// the display's size to begin with, Alt+1..3 for the scale, a double click
// for fullscreen, and the frame rate in the title. Keys, mouse buttons and the
// pointer over the window become the guest's input state; leaving the window
// lets go of everything held, since this host sees keys only while focused.
class LinuxHostPresentation final : public hle::HostPresentation
{
public:
    LinuxHostPresentation();
    ~LinuxHostPresentation() override;

    LinuxHostPresentation(const LinuxHostPresentation&) = delete;
    LinuxHostPresentation& operator=(const LinuxHostPresentation&) = delete;

    // Whether the window opens in fullscreen, as --fullscreen or the profile
    // asks; set before the guest takes the display.
    void SetStartFullscreen(bool fullscreen) { fullscreen_ = fullscreen; }

    bool ShowGuestWindow(std::uint32_t guest_window,
                         std::uint32_t width,
                         std::uint32_t height,
                         std::string* error) override;

    void SetRetainBetweenFrames(bool retain) override;
    bool ClearTarget(std::uint16_t rgb565, std::string* error) override;
    bool ReadTarget(std::uint32_t x,
                    std::uint32_t y,
                    std::uint32_t width,
                    std::uint32_t height,
                    std::span<std::uint8_t> pixels,
                    std::uint32_t pitch,
                    std::string* error) override;
    bool WriteTarget(std::uint32_t x,
                     std::uint32_t y,
                     std::uint32_t width,
                     std::uint32_t height,
                     std::span<const std::uint8_t> pixels,
                     std::uint32_t pitch,
                     std::string* error) override;
    bool Draw(const graphics::LegacyDrawCommand& command,
              const graphics::LegacyFixedFunctionState& state,
              std::uint32_t logical_width,
              std::uint32_t logical_height,
              const graphics::LegacyTextureView* texture,
              std::string* error) override;
    bool Present(std::string* error) override;
    void DiscardTexture(std::uint64_t identity) override;
    bool CloseRequested() const override { return close_requested_; }
    const hle::HostInputState& Input() const override { return input_; }
    // The primary display's desktop mode, read through SDL.
    bool DesktopDisplayMode(hle::HostDisplayMode* mode, std::string* error) const override;

    // True once a host window is open.
    bool opened() const { return backend_ != nullptr; }
    std::uint32_t guest_window() const { return guest_window_; }
    std::uint32_t scale() const { return scale_; }
    bool fullscreen() const { return fullscreen_; }

    // Keeps the window on screen until the user closes it, so a run that has
    // stopped can still be looked at; returns at once when it was closed.
    void HoldUntilClosed();

private:
    // Acts on one window event Present took from the queue (an SDL_Event).
    void HandleEvent(const void* sdl_event);
    // Sizes the window for the current scale, or makes it fullscreen.
    bool ApplyWindowMode(std::string* error);
    // A new scale or fullscreen state, kept only when the window takes it.
    void ChangeWindowMode(std::uint32_t scale, bool fullscreen);

    std::unique_ptr<graphics::Sdl3OpenGlBackend> backend_;
    std::uint32_t guest_window_ = 0;
    std::uint32_t logical_width_ = 0;
    std::uint32_t logical_height_ = 0;
    std::uint32_t scale_ = graphics::kDefaultWindowScale;
    bool fullscreen_ = false;
    bool close_requested_ = false;
    // Whether the backend's software pacing was already reported.
    bool pacing_reported_ = false;
    graphics::FrameRateMeter frame_rate_;
    hle::HostInputState input_;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_HOST_PRESENTATION_H_
