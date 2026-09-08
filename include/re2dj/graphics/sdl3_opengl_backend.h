#ifndef RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
#define RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_

#include <cstdint>
#include <string>

#include "re2dj/graphics/legacy_draw_command.h"
#include "re2dj/graphics/legacy_texture.h"

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
    // Fills the buffer currently being drawn into with one RGB565 colour, for a
    // guest that clears through its display layer rather than by drawing.
    bool ClearRenderTarget(std::uint16_t rgb565_color, std::string* error);
    bool Present(std::string* error);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
