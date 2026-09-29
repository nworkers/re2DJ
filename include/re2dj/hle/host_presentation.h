#ifndef RE2DJ_HLE_HOST_PRESENTATION_H_
#define RE2DJ_HLE_HOST_PRESENTATION_H_

#include <cstdint>
#include <span>
#include <string>

#include "re2dj/graphics/legacy_draw_command.h"
#include "re2dj/graphics/legacy_texture.h"
#include "re2dj/graphics/true_color.h"
#include "re2dj/hle/host_input.h"

namespace re2dj::hle
{

// A display mode as EnumDisplaySettings reports it.
struct HostDisplayMode
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bits_per_pixel = 0;
    std::uint32_t refresh_hz = 0;
};

// What the host shows for the guest: a window on the host desktop standing for
// the guest's own. Platform-neutral; a host that presents implements it and
// hands it to the facades through ImportCallServices::Presentation().
class HostPresentation
{
public:
    virtual ~HostPresentation() = default;

    // Shows guest_window at width x height, creating the host window the
    // first time. False with error when the host cannot show it.
    virtual bool ShowGuestWindow(std::uint32_t guest_window,
                                 std::uint32_t width,
                                 std::uint32_t height,
                                 std::string* error) = 0;

    // How the guest presents: by flipping buffers it owns (each frame starts
    // from the last one's pixels) or by copying whole surfaces.
    virtual void SetRetainBetweenFrames(bool retain) = 0;
    // The guest's frame: clearing the render target to a 5-6-5 color, drawing
    // into it at the guest's logical size, and presenting it. Each is false
    // with error when the host cannot, or before the window is shown.
    virtual bool ClearTarget(std::uint16_t rgb565, std::string* error) = 0;
    // The same clear with an XRGB8888 colour, which a guest clear carries
    // while 32-bit colour is selected. A host without a deeper target clears
    // to the colour narrowed to 5-6-5.
    virtual bool ClearTargetColor(std::uint32_t xrgb, std::string* error)
    {
        return ClearTarget(graphics::NarrowToRgb565(xrgb), error);
    }
    virtual bool Draw(const graphics::LegacyDrawCommand& command,
                      const graphics::LegacyFixedFunctionState& state,
                      std::uint32_t logical_width,
                      std::uint32_t logical_height,
                      const graphics::LegacyTextureView* texture,
                      std::string* error) = 0;
    virtual bool Present(std::string* error) = 0;
    // The render target's 5-6-5 pixels in [x, y, width, height] (guest
    // coordinates, top row first), copied out to or in from rows pitch bytes
    // apart: what the guest sees when it locks the surface it renders into,
    // and what its unlock puts back on the target.
    virtual bool ReadTarget(std::uint32_t x,
                            std::uint32_t y,
                            std::uint32_t width,
                            std::uint32_t height,
                            std::span<std::uint8_t> pixels,
                            std::uint32_t pitch,
                            std::string* error) = 0;
    virtual bool WriteTarget(std::uint32_t x,
                             std::uint32_t y,
                             std::uint32_t width,
                             std::uint32_t height,
                             std::span<const std::uint8_t> pixels,
                             std::uint32_t pitch,
                             std::string* error) = 0;
    // Forgets a texture the host may have kept for a surface that is gone.
    virtual void DiscardTexture(std::uint64_t identity) = 0;
    // Whether the user has asked to close the host window; the run then ends
    // as the Windows host ends the process.
    virtual bool CloseRequested() const = 0;
    // What the user holds on the keyboard and mouse over the host window,
    // and where the pointer is.
    virtual const HostInputState& Input() const = 0;
    // The host desktop's current mode, which the Windows product's guest
    // reads from Windows itself; false when the host cannot tell.
    virtual bool DesktopDisplayMode(HostDisplayMode* mode, std::string* error) const
    {
        static_cast<void>(mode);
        if (error != nullptr) *error = "the host reports no desktop display mode";
        return false;
    }
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_HOST_PRESENTATION_H_
