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
    bool Present(std::string* error);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_SDL3_OPENGL_BACKEND_H_
