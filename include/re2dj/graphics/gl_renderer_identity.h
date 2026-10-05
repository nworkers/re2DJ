#ifndef RE2DJ_GRAPHICS_GL_RENDERER_IDENTITY_H_
#define RE2DJ_GRAPHICS_GL_RENDERER_IDENTITY_H_

#include <string>
#include <string_view>

namespace re2dj::graphics
{

// What draws the picture, as the OSD shows it (#6): the strings the OpenGL
// context reports and the SDL video driver it was created through. Filled by
// the SDL3 OpenGL backend once its context is current; plain values, so the
// OSD and the tests need neither GL nor SDL headers.
struct GlRendererIdentity
{
    std::string renderer;      // GL_RENDERER
    std::string vendor;        // GL_VENDOR
    std::string version;       // GL_VERSION
    std::string video_driver;  // SDL_GetCurrentVideoDriver(): windows, x11, wayland
    bool software = false;     // IsSoftwareGlRenderer(renderer)
};

// Whether a GL_RENDERER string names a software rasterizer: Mesa's llvmpipe,
// softpipe and swrast, Windows' driverless "GDI Generic", WARP ("Microsoft
// Basic Render Driver", reached through D3D12 on WSL) and SwiftShader.
// Case-insensitive substring match. GPU names vary without end, so the list
// names the software side; an unknown or empty string is not software.
bool IsSoftwareGlRenderer(std::string_view renderer);

// Fills `software` from `renderer`, and stands in "unknown" for a null or
// empty string so the OSD never shows a blank line.
GlRendererIdentity MakeGlRendererIdentity(const char* renderer,
                                          const char* vendor,
                                          const char* version,
                                          const char* video_driver);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_GL_RENDERER_IDENTITY_H_
