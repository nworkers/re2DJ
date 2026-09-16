#ifndef RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
#define RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_

#include <cstdint>
#include <string>

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
    bool Present(std::string* error);
    // Installs what Present draws over each composited frame, or removes it
    // when null. Takes effect only after Initialize succeeds, since that is
    // where the backend's state comes into being. The backend does not own the
    // overlay, which must outlive it or be removed first.
    void SetPresentOverlay(PresentOverlay* overlay);
    // The swap interval the driver actually applied, read back after the
    // policy was requested: 1 for vertical sync, 0 for immediate, -1 for
    // adaptive. A driver can refuse a request, so this is the value to report
    // rather than the one in the config. Zero before Initialize succeeds.
    // This layer keeps no log of its own, so the host reads it and records it.
    int applied_swap_interval() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
