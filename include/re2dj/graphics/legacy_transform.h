#ifndef RE2DJ_GRAPHICS_LEGACY_TRANSFORM_H_
#define RE2DJ_GRAPHICS_LEGACY_TRANSFORM_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "re2dj/graphics/legacy_draw_command.h"

namespace re2dj::graphics
{

struct LegacyMatrix4x4
{
    std::array<float, 16> values = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct LegacyViewportTransform
{
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float screen_width = 640.0f;
    float screen_height = 480.0f;
    float clip_x = -1.0f;
    float clip_y = 1.0f;
    float clip_width = 2.0f;
    float clip_height = 2.0f;
    float min_z = 0.0f;
    float max_z = 1.0f;
};

struct LegacyTransformState
{
    LegacyMatrix4x4 world;
    LegacyMatrix4x4 view;
    LegacyMatrix4x4 projection;
    LegacyViewportTransform viewport;
};

bool DecodeUntransformedVertices(std::span<const std::byte> source,
                                 std::size_t vertex_count,
                                 std::uint32_t fvf,
                                 PrimitiveTopology topology,
                                 const LegacyTransformState& transform,
                                 LegacyDrawCommand* command,
                                 std::string* error);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_LEGACY_TRANSFORM_H_
