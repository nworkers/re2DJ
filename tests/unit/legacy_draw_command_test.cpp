#include "re2dj/graphics/legacy_draw_command.h"

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
    float reciprocal_w;
    std::uint32_t diffuse;
    std::uint32_t specular;
    float u;
    float v;
};

static_assert(sizeof(PackedVertex) == re2dj::graphics::kTransformedLitVertexStride);

}  // namespace

void RunLegacyDrawCommandTests(re2dj::test::Context& context)
{
    RE2DJ_CHECK(context, re2dj::graphics::AreLegacyDrawFlagsSupported(0));
    RE2DJ_CHECK(context,
                re2dj::graphics::AreLegacyDrawFlagsSupported(
                    re2dj::graphics::kLegacyDrawWaitFlag));
    RE2DJ_CHECK(context, !re2dj::graphics::AreLegacyDrawFlagsSupported(0x00000002));

    const std::array<PackedVertex, 4> input = {{
        {0.0f, 480.0f, 0.99f, 0.5f, 0xffffffff, 0, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.99f, 0.5f, 0xffffffff, 0, 0.0f, 0.0f},
        {640.0f, 480.0f, 0.99f, 0.5f, 0xffffffff, 0, 1.0f, 1.0f},
        {640.0f, 0.0f, 0.99f, 0.5f, 0xffffffff, 0, 1.0f, 0.0f},
    }};
    std::array<std::byte, sizeof(input)> bytes = {};
    std::memcpy(bytes.data(), input.data(), bytes.size());
    re2dj::graphics::LegacyDrawCommand command;
    std::string error;
    RE2DJ_CHECK(context,
                re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    input.size(),
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    &command,
                    &error));
    RE2DJ_CHECK_EQ(context, command.vertices.size(), std::size_t{4});
    RE2DJ_CHECK_EQ(context, command.vertices[2].x, 640.0f);
    RE2DJ_CHECK_EQ(context, command.vertices[0].diffuse_argb, std::uint32_t{0xffffffff});
    RE2DJ_CHECK_EQ(context,
                   command.topology,
                   re2dj::graphics::PrimitiveTopology::kTriangleStrip);

    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeTransformedLitVertices(
                    std::span<const std::byte>(bytes.data(), bytes.size() - 1),
                    input.size(),
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    &command,
                    &error));
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    2,
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    &command,
                    &error));
    RE2DJ_CHECK(context,
                re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    2,
                    re2dj::graphics::PrimitiveTopology::kLineList,
                    &command,
                    &error));
    RE2DJ_CHECK_EQ(context,
                   command.topology,
                   re2dj::graphics::PrimitiveTopology::kLineList);
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    3,
                    re2dj::graphics::PrimitiveTopology::kLineList,
                    &command,
                    &error));

    auto zero_reciprocal_w = input;
    zero_reciprocal_w[0].reciprocal_w = 0.0f;
    std::memcpy(bytes.data(), zero_reciprocal_w.data(), bytes.size());
    RE2DJ_CHECK(context,
                re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    2,
                    re2dj::graphics::PrimitiveTopology::kLineList,
                    &command,
                    &error));
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    input.size(),
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    &command,
                    &error));

    auto invalid = input;
    invalid[1].u = (std::numeric_limits<float>::quiet_NaN)();
    std::memcpy(bytes.data(), invalid.data(), bytes.size());
    RE2DJ_CHECK(context,
                !re2dj::graphics::DecodeTransformedLitVertices(
                    bytes,
                    invalid.size(),
                    re2dj::graphics::PrimitiveTopology::kTriangleStrip,
                    &command,
                    &error));
}
