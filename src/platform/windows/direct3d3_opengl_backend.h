#ifndef RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_OPENGL_BACKEND_H_
#define RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_OPENGL_BACKEND_H_

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <string>

#include "re2dj/graphics/legacy_draw_command.h"

namespace re2dj::platform::windows
{

struct Rgb565TextureView
{
    const void* pixels = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t pitch = 0;
    bool has_source_color_key = false;
    std::uint16_t source_color_key = 0;
};

class Direct3d3OpenGlBackend
{
public:
    Direct3d3OpenGlBackend();
    ~Direct3d3OpenGlBackend();

    Direct3d3OpenGlBackend(const Direct3d3OpenGlBackend&) = delete;
    Direct3d3OpenGlBackend& operator=(const Direct3d3OpenGlBackend&) = delete;

    bool Initialize(HWND window, std::string* error);
    bool Draw(const graphics::LegacyDrawCommand& command,
              std::uint32_t logical_width,
              std::uint32_t logical_height,
              const Rgb565TextureView* texture,
              std::string* error);
    bool Present(std::string* error);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_OPENGL_BACKEND_H_
