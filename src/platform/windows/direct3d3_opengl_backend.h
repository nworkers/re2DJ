#ifndef RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_OPENGL_BACKEND_H_
#define RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_OPENGL_BACKEND_H_

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <string>

#include "re2dj/graphics/legacy_draw_command.h"
#include "re2dj/graphics/legacy_texture.h"

namespace re2dj::platform::windows
{

class Direct3d3OpenGlBackend
{
public:
    Direct3d3OpenGlBackend();
    ~Direct3d3OpenGlBackend();

    Direct3d3OpenGlBackend(const Direct3d3OpenGlBackend&) = delete;
    Direct3d3OpenGlBackend& operator=(const Direct3d3OpenGlBackend&) = delete;

    bool Initialize(HWND window, std::string* error);
    bool Draw(const graphics::LegacyDrawCommand& command,
              const graphics::LegacyFixedFunctionState& state,
              std::uint32_t logical_width,
              std::uint32_t logical_height,
              const graphics::LegacyTextureView* texture,
              std::string* error);
    void DiscardTexture(std::uint64_t identity);
    bool Present(std::string* error);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_DIRECT3D3_OPENGL_BACKEND_H_
