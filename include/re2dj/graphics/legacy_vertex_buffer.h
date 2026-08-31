#ifndef RE2DJ_GRAPHICS_LEGACY_VERTEX_BUFFER_H_
#define RE2DJ_GRAPHICS_LEGACY_VERTEX_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace re2dj::graphics
{
struct LegacyVertexBufferDesc
{
    std::uint32_t size = 0;
    std::uint32_t caps = 0;
    std::uint32_t fvf = 0;
    std::uint32_t vertex_count = 0;
};

// Flexible vertex format bits from the Direct3D 3-era fixed contract. The
// platform-neutral core restates them so it never includes host SDK headers.
inline constexpr std::uint32_t kFvfXyz = 0x002;
inline constexpr std::uint32_t kFvfXyzRhw = 0x004;
inline constexpr std::uint32_t kFvfNormal = 0x010;
inline constexpr std::uint32_t kFvfReserved1 = 0x020;
inline constexpr std::uint32_t kFvfDiffuse = 0x040;
inline constexpr std::uint32_t kFvfSpecular = 0x080;
inline constexpr std::uint32_t kFvfTextureCountMask = 0xf00;
inline constexpr std::uint32_t kFvfTextureCountShift = 8;

// Returns the byte stride implied by confirmed FVF bits, or zero when the
// format has no recognized position component.
std::uint32_t VertexStrideFromFvf(std::uint32_t fvf);

// Owns the guest-visible vertex storage of one Direct3D vertex buffer. The
// DX6 IDirect3DVertexBuffer lock has no offset or size arguments, so a lock
// always covers the whole storage.
class LegacyVertexBuffer
{
public:
    static std::unique_ptr<LegacyVertexBuffer> Create(const LegacyVertexBufferDesc& descriptor);
    const LegacyVertexBufferDesc& descriptor() const;
    std::uint32_t stride() const;
    std::span<const std::byte> vertices() const;
    bool locked() const;
    std::span<std::byte> Lock();
    bool Unlock();

private:
    LegacyVertexBuffer(const LegacyVertexBufferDesc& descriptor, std::uint32_t stride);
    LegacyVertexBufferDesc descriptor_;
    std::uint32_t stride_ = 0;
    std::vector<std::byte> vertices_;
    bool locked_ = false;
};

bool ExpandIndexedVertices(std::span<const std::byte> source,
                           std::size_t stride,
                           std::size_t vertex_count,
                           std::span<const std::uint16_t> indices,
                           std::vector<std::byte>* expanded);
}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_LEGACY_VERTEX_BUFFER_H_
