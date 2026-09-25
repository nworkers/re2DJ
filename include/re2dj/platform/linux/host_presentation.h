#ifndef RE2DJ_PLATFORM_LINUX_HOST_PRESENTATION_H_
#define RE2DJ_PLATFORM_LINUX_HOST_PRESENTATION_H_

#include <cstdint>
#include <memory>
#include <string>

#include "re2dj/hle/host_presentation.h"

namespace re2dj::graphics
{
class Sdl3OpenGlBackend;
}

namespace re2dj::platform::linux
{

// The Linux host's presentation: one SDL3/OpenGL window, made the first time
// the guest takes the display, titled like the Windows product's window.
// Nothing is drawn into it yet; it shows black until the DirectX core's draw
// phases reach Linux.
class LinuxHostPresentation final : public hle::HostPresentation
{
public:
    LinuxHostPresentation();
    ~LinuxHostPresentation() override;

    LinuxHostPresentation(const LinuxHostPresentation&) = delete;
    LinuxHostPresentation& operator=(const LinuxHostPresentation&) = delete;

    bool ShowGuestWindow(std::uint32_t guest_window,
                         std::uint32_t width,
                         std::uint32_t height,
                         std::string* error) override;

    // True once a host window is open.
    bool opened() const { return backend_ != nullptr; }
    std::uint32_t guest_window() const { return guest_window_; }

    // Keeps the window on screen until the user closes it, so a run that has
    // stopped can still be looked at.
    void HoldUntilClosed();

private:
    std::unique_ptr<graphics::Sdl3OpenGlBackend> backend_;
    std::uint32_t guest_window_ = 0;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_HOST_PRESENTATION_H_
