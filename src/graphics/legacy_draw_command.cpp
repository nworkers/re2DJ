#include "re2dj/graphics/legacy_draw_command.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace re2dj::graphics
{
namespace
{

struct PackedTransformedLitVertex
{
    float x;
    float y;
    float z;
    float reciprocal_w;
    std::uint32_t diffuse_argb;
    std::uint32_t specular_argb;
    float texture_u;
    float texture_v;
};

static_assert(sizeof(PackedTransformedLitVertex) == kTransformedLitVertexStride);

bool IsFinite(const PackedTransformedLitVertex& vertex)
{
    return std::isfinite(vertex.x) && std::isfinite(vertex.y) &&
           std::isfinite(vertex.z) && std::isfinite(vertex.reciprocal_w) &&
           std::isfinite(vertex.texture_u) && std::isfinite(vertex.texture_v);
}

}  // namespace

bool AreLegacyDrawFlagsSupported(std::uint32_t flags)
{
    return (flags & ~kLegacyDrawWaitFlag) == 0;
}

bool DecodeTransformedLitVertices(std::span<const std::byte> source,
                                  std::size_t vertex_count,
                                  PrimitiveTopology topology,
                                  LegacyDrawCommand* command,
                                  std::string* error)
{
    if (command == nullptr || error == nullptr)
    {
        return false;
    }
    command->vertices.clear();
    const std::size_t minimum_vertex_count =
        topology == PrimitiveTopology::kLineList ? 2 : 3;
    if (vertex_count < minimum_vertex_count ||
        (topology == PrimitiveTopology::kLineList && vertex_count % 2 != 0) ||
        vertex_count > (std::numeric_limits<std::size_t>::max)() /
                           kTransformedLitVertexStride)
    {
        *error = "invalid transformed/lit vertex count";
        return false;
    }
    const std::size_t required = vertex_count * kTransformedLitVertexStride;
    if (source.size() < required)
    {
        *error = "transformed/lit vertex buffer is truncated";
        return false;
    }

    command->topology = topology;
    command->vertices.reserve(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index)
    {
        PackedTransformedLitVertex packed = {};
        std::memcpy(&packed,
                    source.data() + index * kTransformedLitVertexStride,
                    sizeof(packed));
        if (!IsFinite(packed))
        {
            command->vertices.clear();
            *error = "transformed/lit vertex contains an invalid float";
            return false;
        }
        if (packed.reciprocal_w == 0.0f && topology != PrimitiveTopology::kLineList)
        {
            command->vertices.clear();
            *error = "transformed/lit vertex has zero reciprocal w";
            return false;
        }
        command->vertices.push_back({packed.x,
                                     packed.y,
                                     packed.z,
                                     packed.reciprocal_w,
                                     packed.diffuse_argb,
                                     packed.specular_argb,
                                     packed.texture_u,
                                     packed.texture_v});
    }
    error->clear();
    return true;
}

}  // namespace re2dj::graphics
