#include "re2dj/graphics/legacy_vertex_buffer.h"

#include <array>
#include <cstring>

#include "test_support.h"

void RunLegacyVertexBufferTests(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context,
                   re2dj::graphics::VertexStrideFromFvf(re2dj::graphics::kFvfXyz),
                   std::uint32_t{12});
    RE2DJ_CHECK_EQ(context,
                   re2dj::graphics::VertexStrideFromFvf(re2dj::graphics::kFvfXyzRhw),
                   std::uint32_t{16});
    const std::uint32_t transformed_lit = re2dj::graphics::kFvfXyzRhw |
                                          re2dj::graphics::kFvfDiffuse |
                                          re2dj::graphics::kFvfSpecular | 0x100;
    RE2DJ_CHECK_EQ(context,
                   re2dj::graphics::VertexStrideFromFvf(transformed_lit),
                   std::uint32_t{32});
    RE2DJ_CHECK_EQ(context,
                   re2dj::graphics::VertexStrideFromFvf(re2dj::graphics::kFvfXyz |
                                                            re2dj::graphics::kFvfNormal | 0x100),
                   std::uint32_t{32});
    RE2DJ_CHECK_EQ(context,
                   re2dj::graphics::VertexStrideFromFvf(
                       re2dj::graphics::kFvfXyz | re2dj::graphics::kFvfReserved1 |
                       re2dj::graphics::kFvfDiffuse | re2dj::graphics::kFvfSpecular | 0x100),
                   std::uint32_t{32});
    RE2DJ_CHECK_EQ(context, re2dj::graphics::VertexStrideFromFvf(0x140), std::uint32_t{0});
    RE2DJ_CHECK_EQ(context,
                   re2dj::graphics::VertexStrideFromFvf(transformed_lit | 0x900),
                   std::uint32_t{0});

    re2dj::graphics::LegacyVertexBufferDesc descriptor;
    descriptor.size = 16;
    descriptor.caps = 0x800;
    descriptor.fvf = transformed_lit;
    descriptor.vertex_count = 4;
    auto buffer = re2dj::graphics::LegacyVertexBuffer::Create(descriptor);
    RE2DJ_CHECK(context, buffer != nullptr);
    if (buffer == nullptr) return;
    RE2DJ_CHECK_EQ(context, buffer->stride(), std::uint32_t{32});
    RE2DJ_CHECK_EQ(context, buffer->descriptor().vertex_count, std::uint32_t{4});

    std::span<std::byte> vertices = buffer->Lock();
    RE2DJ_CHECK_EQ(context, vertices.size(), std::size_t{128});
    RE2DJ_CHECK(context, buffer->locked());
    if (!vertices.empty()) std::memset(vertices.data(), 0xab, vertices.size());
    RE2DJ_CHECK(context, buffer->Lock().empty());
    RE2DJ_CHECK(context, buffer->Unlock());
    RE2DJ_CHECK(context, !buffer->locked());
    RE2DJ_CHECK(context, !buffer->Unlock());
    vertices = buffer->Lock();
    RE2DJ_CHECK_EQ(context, vertices.size(), std::size_t{128});
    RE2DJ_CHECK_EQ(context, static_cast<unsigned char>(vertices[127]), 0xab);
    buffer->Unlock();
    RE2DJ_CHECK_EQ(context, buffer->vertices().size(), std::size_t{128});

    const std::array<std::uint16_t, 6> indices = {0, 1, 2, 2, 1, 3};
    std::vector<std::byte> expanded;
    RE2DJ_CHECK(context,
                re2dj::graphics::ExpandIndexedVertices(buffer->vertices(),
                                                       buffer->stride(),
                                                       buffer->descriptor().vertex_count,
                                                       indices,
                                                       &expanded));
    RE2DJ_CHECK_EQ(context, expanded.size(), std::size_t{192});
    RE2DJ_CHECK(context,
                std::memcmp(expanded.data() + 3 * buffer->stride(),
                            buffer->vertices().data() + 2 * buffer->stride(),
                            buffer->stride()) == 0);
    const std::array<std::uint16_t, 1> invalid_indices = {4};
    RE2DJ_CHECK(context,
                !re2dj::graphics::ExpandIndexedVertices(buffer->vertices(),
                                                        buffer->stride(),
                                                        buffer->descriptor().vertex_count,
                                                        invalid_indices,
                                                        &expanded));
    RE2DJ_CHECK(context, expanded.empty());

    re2dj::graphics::LegacyVertexBufferDesc invalid = descriptor;
    invalid.fvf = 0x140;
    RE2DJ_CHECK(context, re2dj::graphics::LegacyVertexBuffer::Create(invalid) == nullptr);
    invalid = descriptor;
    invalid.vertex_count = 0;
    RE2DJ_CHECK(context, re2dj::graphics::LegacyVertexBuffer::Create(invalid) == nullptr);
}
