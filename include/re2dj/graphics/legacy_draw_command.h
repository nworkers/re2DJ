#ifndef RE2DJ_GRAPHICS_LEGACY_DRAW_COMMAND_H_
#define RE2DJ_GRAPHICS_LEGACY_DRAW_COMMAND_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace re2dj::graphics
{

enum class PrimitiveTopology
{
    kTriangleStrip,
};

struct TransformedLitVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float reciprocal_w = 1.0f;
    std::uint32_t diffuse_argb = 0xffffffff;
    std::uint32_t specular_argb = 0;
    float texture_u = 0.0f;
    float texture_v = 0.0f;
};

struct LegacyDrawCommand
{
    PrimitiveTopology topology = PrimitiveTopology::kTriangleStrip;
    std::vector<TransformedLitVertex> vertices;
};

inline constexpr std::size_t kTransformedLitVertexStride = 32;

bool DecodeTransformedLitVertices(std::span<const std::byte> source,
                                  std::size_t vertex_count,
                                  LegacyDrawCommand* command,
                                  std::string* error);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_LEGACY_DRAW_COMMAND_H_
