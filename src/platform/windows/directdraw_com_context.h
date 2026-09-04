#pragma once

#include <windows.h>
#include <cstdint>

namespace re2dj::graphics
{
class Sdl3OpenGlBackend;
}

namespace re2dj::platform::windows
{

constexpr DWORD kDirectDrawRootMagic = 0x52324444;       // 'R2DD'
constexpr DWORD kDirectDrawSurfaceMagic = 0x52325346;    // 'R2SF'
constexpr DWORD kDirect3DDeviceMagic = 0x52324456;       // 'R2DV'
constexpr DWORD kDirect3DViewportMagic = 0x52325650;     // 'R2VP'
constexpr DWORD kDirect3DVertexBufferMagic = 0x52325642; // 'R2VB'

struct DirectDrawComContext
{
    DWORD magic = kDirectDrawRootMagic;
    DWORD width = 640;
    DWORD height = 480;
    DWORD bits_per_pixel = 16;
    HWND window = nullptr;
    std::uint64_t next_texture_identity = 1;
    std::uint32_t next_surface_diagnostic_id = 1;
    std::uint64_t next_composition_diagnostic_sequence = 1;
    std::uint32_t create_surface_diagnostic_count = 0;
    std::uint32_t surface_dc_diagnostic_count = 0;
    std::uint32_t source_blt_diagnostic_count = 0;
    std::uint32_t source_blt_target_diagnostic_count = 0;
    std::uint32_t texture_load_diagnostic_count = 0;
    std::uint32_t color_fill_diagnostic_count = 0;
    std::uint32_t flip_diagnostic_count = 0;
    std::uint32_t draw_failure_diagnostic_count = 0;
    std::uint32_t untextured_draw_diagnostic_count = 0;
    std::uint32_t late_draw_diagnostic_count = 0;
    std::uint32_t late_draw_target_diagnostic_count = 0;
    std::uint32_t transform_diagnostic_count = 0;
    std::uint64_t frame_number = 0;
    LARGE_INTEGER fps_frequency = {};
    LARGE_INTEGER fps_interval_start = {};
    std::uint32_t fps_interval_frames = 0;
    re2dj::graphics::Sdl3OpenGlBackend* render_backend = nullptr;
};

}  // namespace re2dj::platform::windows
