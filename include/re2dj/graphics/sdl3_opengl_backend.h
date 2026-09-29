#ifndef RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
#define RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_

#include <cstdint>
#include <functional>
#include <span>
#include <string>

#include "re2dj/graphics/color_depth.h"
#include "re2dj/graphics/legacy_draw_command.h"
#include "re2dj/graphics/legacy_texture.h"
#include "re2dj/graphics/present_overlay.h"
#include "re2dj/graphics/present_sync.h"

namespace re2dj::graphics
{

struct Sdl3OpenGlWindowConfig
{
    void* native_window = nullptr;
    std::uint32_t width = 640;
    std::uint32_t height = 480;
    const char* title = "re2DJ";
    // Keeps the per-draw OpenGL error check on for the whole session. It is a
    // pipeline synchronization point in some drivers, so the product path
    // leaves it off and relies on the initial draws and the per-frame check
    // in Present. The host decides; this layer stays platform-neutral.
    bool draw_diagnostics = false;
    // Whether a frame starts from what the last one left behind. A guest that
    // presents by flipping keeps drawing into buffers it owns, so a frame in
    // which it redraws only part of the screen needs the rest still there. A
    // guest that presents by copying a whole surface over the screen overwrites
    // everything each time and starts from nothing.
    bool retain_between_frames = false;
    // When a present returns. The default keeps the behavior the product had
    // before this became explicit, so a caller that leaves it alone sees no
    // change.
    PresentSync present_sync = PresentSync::kVerticalSync;
    // Last, so callers that list the fields in order keep their meaning.
    // For a window this backend makes (no native_window): whether the user
    // can resize it, and whether it opens centred on its display.
    bool resizable = false;
    bool centered = false;
};

class Sdl3OpenGlBackend
{
public:
    Sdl3OpenGlBackend();
    ~Sdl3OpenGlBackend();

    Sdl3OpenGlBackend(const Sdl3OpenGlBackend&) = delete;
    Sdl3OpenGlBackend& operator=(const Sdl3OpenGlBackend&) = delete;

    bool Initialize(const Sdl3OpenGlWindowConfig& config, std::string* error);
    bool Draw(const LegacyDrawCommand& command, const LegacyFixedFunctionState& state,
              std::uint32_t logical_width, std::uint32_t logical_height,
              const LegacyTextureView* texture, std::string* error);
    void DiscardTexture(std::uint64_t identity);
    // Fills the logical render target with one RGB565 color for an explicit
    // guest clear that does not arrive as a draw command.
    bool ClearRenderTarget(std::uint16_t rgb565_color, std::string* error);
    // The same with an XRGB8888 colour, for a clear whose guest colour is
    // 8 bits per channel while 32-bit colour is selected.
    bool ClearRenderTargetColor(std::uint32_t xrgb, std::string* error);
    // Copies the logical render target's RGB565 pixels in [x, y, width,
    // height] (guest coordinates, top row first) out to, or in from, rows
    // pitch bytes apart starting at the rectangle's first pixel: what a guest
    // Lock of the surface it renders into sees, and what its Unlock puts back.
    // On a 32-bit target a read narrows the colours as a true-color plane
    // does, and a write keeps every pixel that still narrows to what the guest
    // wrote (true_color.h).
    bool ReadRenderTarget(std::uint32_t x,
                          std::uint32_t y,
                          std::uint32_t width,
                          std::uint32_t height,
                          std::span<std::uint8_t> pixels,
                          std::uint32_t pitch,
                          std::string* error);
    bool WriteRenderTarget(std::uint32_t x,
                           std::uint32_t y,
                           std::uint32_t width,
                           std::uint32_t height,
                           std::span<const std::uint8_t> pixels,
                           std::uint32_t pitch,
                           std::string* error);
    bool Present(std::string* error);
    // Installs what Present draws over each composited frame, or removes it
    // when null. Takes effect only after Initialize succeeds, since that is
    // where the backend's state comes into being. The backend does not own the
    // overlay, which must outlive it or be removed first.
    void SetPresentOverlay(PresentOverlay* overlay);
    // Changes Sdl3OpenGlWindowConfig::retain_between_frames once the window
    // is open, for a host that learns how the guest presents only after the
    // window is shown.
    void SetRetainBetweenFrames(bool retain);
    // Called with each window event Present takes from the queue, for a host
    // that acts on them itself; without one they are dropped, as the Windows
    // host has its own window procedure. The argument is an SDL_Event.
    void SetEventObserver(std::function<void(const void* sdl_event)> observer);
    // Window controls for a window this backend made: its size in window
    // coordinates, centred again on its display; monitor-sized borderless
    // fullscreen on or off; and its title.
    bool ResizeWindow(std::uint32_t width, std::uint32_t height, std::string* error);
    bool SetFullscreen(bool fullscreen, std::string* error);
    void SetTitle(const char* title);
    // The swap interval the driver actually applied, read back after the
    // policy was requested: 1 for vertical sync, 0 for immediate, -1 for
    // adaptive. A driver can refuse a request, so this is the value to report
    // rather than the one in the config. Zero before Initialize succeeds.
    // This layer keeps no log of its own, so the host reads it and records it.
    int applied_swap_interval() const;
    // True once presents were found not to block despite the policy, so the
    // backend paces them at the display rate itself (present_pacer.h). The
    // host reads it and records when it turns on.
    bool software_pacing_engaged() const;
    // The logical render target's colour depth. It follows the process's
    // selection (color_depth.h) from the next operation after a change, and
    // stays 16-bit when the driver cannot render into RGB8, which
    // true_color_unavailable() then reports for the host to record. While it
    // is 32-bit, textures take their colours from the surfaces' true-color
    // planes.
    ColorDepth render_target_depth() const;
    bool true_color_unavailable() const;

    // The primary display's desktop mode, read without a window: its size,
    // bits per pixel (bytes per pixel times eight, as Windows reports a
    // 32-bit desktop), and refresh rate rounded to whole hertz. False with
    // error when SDL video cannot start or reports no display.
    static bool QueryDesktopDisplayMode(std::uint32_t* width,
                                        std::uint32_t* height,
                                        std::uint32_t* bits_per_pixel,
                                        std::uint32_t* refresh_hz,
                                        std::string* error);

private:
    bool ClearTo(float red, float green, float blue, std::string* error);

    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
