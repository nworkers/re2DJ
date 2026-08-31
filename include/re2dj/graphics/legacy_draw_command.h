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
    kTriangleList,
    kLineList,
};

enum class BlendFactor
{
    kZero,
    kOne,
    kSourceColor,
    kInverseSourceColor,
    kSourceAlpha,
    kInverseSourceAlpha,
};

enum class CompareFunction
{
    kNever,
    kLess,
    kEqual,
    kLessEqual,
    kGreater,
    kNotEqual,
    kGreaterEqual,
    kAlways,
};

enum class TextureFilter
{
    kNearest,
    kLinear,
};

struct LegacyFixedFunctionState
{
    bool color_key_enabled = false;
    bool alpha_test_enabled = false;
    CompareFunction alpha_function = CompareFunction::kNotEqual;
    std::uint8_t alpha_reference = 0;
    bool alpha_blend_enabled = false;
    BlendFactor source_blend = BlendFactor::kOne;
    BlendFactor destination_blend = BlendFactor::kZero;
    bool depth_test_enabled = false;
    bool depth_write_enabled = false;
    CompareFunction depth_function = CompareFunction::kLessEqual;
    bool fade_compatibility_applied = false;
    TextureFilter minification_filter = TextureFilter::kNearest;
    TextureFilter magnification_filter = TextureFilter::kNearest;
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
inline constexpr std::uint32_t kLegacyDrawWaitFlag = 0x00000001;

bool AreLegacyDrawFlagsSupported(std::uint32_t flags);

bool DecodeTransformedLitVertices(std::span<const std::byte> source,
                                  std::size_t vertex_count,
                                  PrimitiveTopology topology,
                                  LegacyDrawCommand* command,
                                  std::string* error);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_LEGACY_DRAW_COMMAND_H_
