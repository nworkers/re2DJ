#include "re2dj/graphics/legacy_transform.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#include "test_support.h"

namespace
{

struct PackedVertex
{
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

struct PackedLitVertex
{
    float x;
    float y;
    float z;
    std::uint32_t reserved;
    std::uint32_t diffuse;
    std::uint32_t specular;
    float u;
    float v;
};

static_assert(sizeof(PackedVertex) == 32);
static_assert(sizeof(PackedLitVertex) == 32);

bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.0001f;
}

}  // namespace

void RunLegacyTransformTests(re2dj::test::Context& context)
{
    const std::array<PackedVertex, 4> vertices = {{
        {-1.0f, -1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        {-1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, -1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f},
    }};
    std::array<std::byte, sizeof(vertices)> bytes = {};
    std::memcpy(bytes.data(), vertices.data(), bytes.size());
    re2dj::graphics::LegacyTransformState transform;
    re2dj::graphics::LegacyDrawCommand command;
    std::string error;
    RE2DJ_CHECK(context,
                re2dj::graphics::DecodeUntransformedVertices(
                    bytes,
                    vertices.size(),
                    0x112,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    transform,
                    &command,
                    &error));
    RE2DJ_CHECK_EQ(context, command.vertices.size(), std::size_t{4});
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].x, 0.0f));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].y, 480.0f));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[3].x, 640.0f));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[3].y, 0.0f));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].z, 0.5f));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].reciprocal_w, 1.0f));
    RE2DJ_CHECK_EQ(context, command.vertices[0].diffuse_argb, std::uint32_t{0xffffffff});
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[2].texture_u, 1.0f));

    re2dj::graphics::LegacyTransformState translated;
    translated.world.values[12] = 0.25f;
    translated.view.values[12] = 0.25f;
    translated.projection.values[12] = 0.5f;
    RE2DJ_CHECK(context,
                re2dj::graphics::DecodeUntransformedVertices(
                    bytes,
                    vertices.size(),
                    0x112,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    translated,
                    &command,
                    &error));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].x, 320.0f));

    const std::array<PackedLitVertex, 4> lit_vertices = {{
        {0.0f, 0.0f, 0.25f, 0x12345678, 0xff102030, 0xff405060, 0.0f, 0.0f},
        {0.0f, 0.5f, 0.25f, 0, 0xff112233, 0, 0.0f, 1.0f},
        {0.5f, 0.0f, 0.25f, 0, 0xff223344, 0, 1.0f, 0.0f},
        {0.5f, 0.5f, 0.25f, 0, 0xff334455, 0, 1.0f, 1.0f},
    }};
    std::array<std::byte, sizeof(lit_vertices)> lit_bytes = {};
    std::memcpy(lit_bytes.data(), lit_vertices.data(), lit_bytes.size());
    RE2DJ_CHECK(context,
                re2dj::graphics::DecodeUntransformedVertices(
                    lit_bytes,
                    lit_vertices.size(),
                    0x1e2,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    transform,
                    &command,
                    &error));
    RE2DJ_CHECK_EQ(context, command.vertices[0].diffuse_argb, std::uint32_t{0xff102030});
    RE2DJ_CHECK_EQ(context, command.vertices[0].specular_argb, std::uint32_t{0xff405060});
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].x, 320.0f));
    RE2DJ_CHECK(context, NearlyEqual(command.vertices[0].y, 240.0f));

    auto invalid_vertices = vertices;
    invalid_vertices[0].x = (std::numeric_limits<float>::quiet_NaN)();
    std::memcpy(bytes.data(), invalid_vertices.data(), bytes.size());
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeUntransformedVertices(
                    bytes,
                    invalid_vertices.size(),
                    0x112,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    transform,
                    &command,
                    &error));
    std::memcpy(bytes.data(), vertices.data(), bytes.size());
    transform.projection.values[15] = 0.0f;
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeUntransformedVertices(
                    bytes,
                    vertices.size(),
                    0x112,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    transform,
                    &command,
                    &error));
    transform = {};
    transform.viewport.clip_width = 0.0f;
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeUntransformedVertices(
                    bytes,
                    vertices.size(),
                    0x112,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    transform,
                    &command,
                    &error));
}
