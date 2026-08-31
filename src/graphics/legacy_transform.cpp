#include "re2dj/graphics/legacy_transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "re2dj/graphics/legacy_vertex_buffer.h"

namespace re2dj::graphics
{
namespace
{

inline constexpr std::uint32_t kFvfVertex =
    kFvfXyz | kFvfNormal | 0x100;
inline constexpr std::uint32_t kFvfLitVertex =
    kFvfXyz | kFvfReserved1 | kFvfDiffuse | kFvfSpecular | 0x100;

struct Vector4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

bool IsFinite(const Vector4& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

bool IsFinite(const LegacyMatrix4x4& matrix)
{
    return std::all_of(matrix.values.begin(), matrix.values.end(), [](float value) {
        return std::isfinite(value);
    });
}

bool IsValid(const LegacyViewportTransform& viewport)
{
    const std::array<float, 10> values = {
        viewport.screen_x, viewport.screen_y, viewport.screen_width,
        viewport.screen_height, viewport.clip_x, viewport.clip_y,
        viewport.clip_width, viewport.clip_height, viewport.min_z,
        viewport.max_z};
    return std::all_of(values.begin(), values.end(), [](float value) {
               return std::isfinite(value);
           }) &&
           viewport.screen_width > 0.0f && viewport.screen_height > 0.0f &&
           viewport.clip_width > 0.0f && viewport.clip_height > 0.0f;
}

Vector4 TransformRowVector(const Vector4& input, const LegacyMatrix4x4& matrix)
{
    const auto& m = matrix.values;
    return {
        input.x * m[0] + input.y * m[4] + input.z * m[8] + input.w * m[12],
        input.x * m[1] + input.y * m[5] + input.z * m[9] + input.w * m[13],
        input.x * m[2] + input.y * m[6] + input.z * m[10] + input.w * m[14],
        input.x * m[3] + input.y * m[7] + input.z * m[11] + input.w * m[15],
    };
}

template <typename T>
T ReadValue(const std::byte* source)
{
    T value = {};
    std::memcpy(&value, source, sizeof(value));
    return value;
}

}  // namespace

bool DecodeUntransformedVertices(std::span<const std::byte> source,
                                 std::size_t vertex_count,
                                 std::uint32_t fvf,
                                 PrimitiveTopology topology,
                                 const LegacyTransformState& transform,
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
    const std::uint32_t stride = VertexStrideFromFvf(fvf);
    if ((fvf != kFvfVertex && fvf != kFvfLitVertex) || stride != 32 ||
        vertex_count < minimum_vertex_count ||
        (topology == PrimitiveTopology::kLineList && vertex_count % 2 != 0) ||
        (topology == PrimitiveTopology::kTriangleList && vertex_count % 3 != 0) ||
        vertex_count > (std::numeric_limits<std::size_t>::max)() / stride)
    {
        *error = "unsupported untransformed vertex format or count";
        return false;
    }
    const std::size_t required = vertex_count * stride;
    if (source.size() < required)
    {
        *error = "untransformed vertex buffer is truncated";
        return false;
    }
    if (!IsFinite(transform.world) || !IsFinite(transform.view) ||
        !IsFinite(transform.projection) || !IsValid(transform.viewport))
    {
        *error = "invalid Direct3D transform or viewport";
        return false;
    }

    command->topology = topology;
    command->vertices.reserve(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index)
    {
        const std::byte* const packed = source.data() + index * stride;
        Vector4 position = {ReadValue<float>(packed),
                            ReadValue<float>(packed + 4),
                            ReadValue<float>(packed + 8),
                            1.0f};
        const std::size_t color_offset = fvf == kFvfLitVertex ? 16 : 0;
        const std::uint32_t diffuse =
            fvf == kFvfLitVertex ? ReadValue<std::uint32_t>(packed + color_offset)
                                 : 0xffffffff;
        const std::uint32_t specular =
            fvf == kFvfLitVertex ? ReadValue<std::uint32_t>(packed + color_offset + 4) : 0;
        const float texture_u = ReadValue<float>(packed + 24);
        const float texture_v = ReadValue<float>(packed + 28);
        if (!IsFinite(position) || !std::isfinite(texture_u) || !std::isfinite(texture_v))
        {
            command->vertices.clear();
            *error = "untransformed vertex contains an invalid float";
            return false;
        }

        position = TransformRowVector(position, transform.world);
        position = TransformRowVector(position, transform.view);
        position = TransformRowVector(position, transform.projection);
        if (!IsFinite(position) || position.w == 0.0f)
        {
            command->vertices.clear();
            *error = "Direct3D vertex transform produced invalid clip coordinates";
            return false;
        }
        const float reciprocal_w = 1.0f / position.w;
        const float normalized_x = position.x * reciprocal_w;
        const float normalized_y = position.y * reciprocal_w;
        const float normalized_z = position.z * reciprocal_w;
        const auto& viewport = transform.viewport;
        const float screen_x = viewport.screen_x +
                               (normalized_x - viewport.clip_x) *
                                   viewport.screen_width / viewport.clip_width;
        const float screen_y = viewport.screen_y +
                               (viewport.clip_y - normalized_y) *
                                   viewport.screen_height / viewport.clip_height;
        const float screen_z = viewport.min_z +
                               normalized_z * (viewport.max_z - viewport.min_z);
        const Vector4 screen = {screen_x, screen_y, screen_z, reciprocal_w};
        if (!IsFinite(screen))
        {
            command->vertices.clear();
            *error = "Direct3D viewport transform produced invalid coordinates";
            return false;
        }
        command->vertices.push_back({screen_x,
                                     screen_y,
                                     screen_z,
                                     reciprocal_w,
                                     diffuse,
                                     specular,
                                     texture_u,
                                     texture_v});
    }
    error->clear();
    return true;
}

}  // namespace re2dj::graphics
