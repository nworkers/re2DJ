#include "re2dj/graphics/legacy_vertex_buffer.h"

#include <limits>

namespace re2dj::graphics
{
std::uint32_t VertexStrideFromFvf(std::uint32_t fvf)
{
    std::uint32_t stride = 0;
    if ((fvf & kFvfXyzRhw) != 0)
    {
        stride = 16;
    }
    else if ((fvf & kFvfXyz) != 0)
    {
        stride = 12;
        if ((fvf & kFvfNormal) != 0) stride += 12;
    }
    else
    {
        return 0;
    }
    if ((fvf & kFvfDiffuse) != 0) stride += 4;
    if ((fvf & kFvfSpecular) != 0) stride += 4;
    // Each texture-coordinate set carries a default two-float pair; explicit
    // per-set size bits have not been observed in this executable yet.
    const std::uint32_t texture_count = (fvf & kFvfTextureCountMask) >> kFvfTextureCountShift;
    if (texture_count > 8) return 0;
    stride += texture_count * 8;
    return stride;
}

LegacyVertexBuffer::LegacyVertexBuffer(const LegacyVertexBufferDesc& descriptor, std::uint32_t stride)
    : descriptor_(descriptor), stride_(stride), vertices_(descriptor.vertex_count * stride) {}

std::unique_ptr<LegacyVertexBuffer> LegacyVertexBuffer::Create(const LegacyVertexBufferDesc& descriptor)
{
    const std::uint32_t stride = VertexStrideFromFvf(descriptor.fvf);
    if (stride == 0 || descriptor.vertex_count == 0) return nullptr;
    const std::uint64_t byte_count =
        static_cast<std::uint64_t>(descriptor.vertex_count) * static_cast<std::uint64_t>(stride);
    if (byte_count > (std::numeric_limits<std::size_t>::max)()) return nullptr;
    return std::unique_ptr<LegacyVertexBuffer>(new LegacyVertexBuffer(descriptor, stride));
}

const LegacyVertexBufferDesc& LegacyVertexBuffer::descriptor() const { return descriptor_; }
std::uint32_t LegacyVertexBuffer::stride() const { return stride_; }

std::span<std::byte> LegacyVertexBuffer::Lock()
{
    if (locked_ || vertices_.empty()) return {};
    locked_ = true;
    return vertices_;
}

bool LegacyVertexBuffer::Unlock()
{
    if (!locked_) return false;
    locked_ = false;
    return true;
}
}  // namespace re2dj::graphics
