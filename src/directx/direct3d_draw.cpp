#include "re2dj/directx/direct3d_draw.h"

#include <algorithm>
#include <cmath>

#include "re2dj/graphics/legacy_vertex_buffer.h"

namespace re2dj::directx
{
namespace
{

using graphics::BlendFactor;
using graphics::CompareFunction;
using graphics::TextureAddressMode;
using graphics::TextureFilter;

bool ConvertCompare(std::uint32_t value, CompareFunction* output)
{
    constexpr CompareFunction kFunctions[] = {
        CompareFunction::kNever,   CompareFunction::kLess,     CompareFunction::kEqual,
        CompareFunction::kLessEqual, CompareFunction::kGreater, CompareFunction::kNotEqual,
        CompareFunction::kGreaterEqual, CompareFunction::kAlways,
    };
    if (value < kD3dCmpNever || value > kD3dCmpAlways)
    {
        return false;
    }
    *output = kFunctions[value - kD3dCmpNever];
    return true;
}

bool ConvertFilter(std::uint32_t value, TextureFilter* output)
{
    if (value == kD3dTfnPoint)
    {
        *output = TextureFilter::kNearest;
        return true;
    }
    if (value == kD3dTfnLinear)
    {
        *output = TextureFilter::kLinear;
        return true;
    }
    return false;
}

bool ConvertAddress(std::uint32_t value, TextureAddressMode* output)
{
    switch (value)
    {
    case kD3dTAddressWrap:
        *output = TextureAddressMode::kWrap;
        return true;
    case kD3dTAddressMirror:
        *output = TextureAddressMode::kMirror;
        return true;
    case kD3dTAddressClamp:
        *output = TextureAddressMode::kClamp;
        return true;
    default:
        return false;
    }
}

void CopyMatrix(const D3dMatrix& source, graphics::LegacyMatrix4x4* destination)
{
    destination->values = source.values;
}

bool IsFullScreenBlackFadeCandidate(const graphics::LegacyDrawCommand& command,
                                    bool textured,
                                    std::uint32_t logical_width,
                                    std::uint32_t logical_height,
                                    bool guest_blend_is_explicit)
{
    if (textured || command.topology != graphics::PrimitiveTopology::kTriangleStrip ||
        command.vertices.size() != 4 || logical_width == 0 || logical_height == 0)
    {
        return false;
    }
    constexpr float kPositionTolerance = 0.01f;
    float minimum_x = command.vertices.front().x;
    float minimum_y = command.vertices.front().y;
    float maximum_x = minimum_x;
    float maximum_y = minimum_y;
    const std::uint32_t first_color = command.vertices.front().diffuse_argb;
    const std::uint32_t first_rgb = first_color & 0x00ffffffU;
    const std::uint32_t first_alpha = first_color >> 24;
    // An opaque quad only counts when the guest named the blend itself. Under
    // its own factors an alpha of 0xff can be a no-op - 1st SE fades with
    // ZERO/SRCALPHA, where the first step, alpha 0xff, means dst*1.0 and must
    // leave the screen alone - whereas a guest that named no blend really is
    // asking for an opaque black fill, and treating that as a fade would erase
    // a legitimate clear.
    if (first_rgb != 0 || first_alpha == 0 || (first_alpha == 0xff && !guest_blend_is_explicit))
    {
        return false;
    }
    for (const auto& vertex : command.vertices)
    {
        minimum_x = std::min(minimum_x, vertex.x);
        minimum_y = std::min(minimum_y, vertex.y);
        maximum_x = std::max(maximum_x, vertex.x);
        maximum_y = std::max(maximum_y, vertex.y);
        if ((vertex.diffuse_argb & 0x00ffffffU) != 0 || (vertex.diffuse_argb >> 24) != first_alpha)
        {
            return false;
        }
    }
    return std::fabs(minimum_x) <= kPositionTolerance && std::fabs(minimum_y) <= kPositionTolerance &&
           std::fabs(maximum_x - static_cast<float>(logical_width)) <= kPositionTolerance &&
           std::fabs(maximum_y - static_cast<float>(logical_height)) <= kPositionTolerance;
}

}  // namespace

DrawPlan PlanDrawPrimitive(std::uint32_t primitive, std::uint32_t fvf, std::uint32_t vertex_count, std::uint32_t flags)
{
    DrawPlan plan;
    const bool strip = primitive == kD3dPtTriangleStrip && vertex_count >= 3;
    const bool list = primitive == kD3dPtTriangleList && vertex_count >= 3 && vertex_count % 3 == 0;
    const bool lines = primitive == kD3dPtLineList && vertex_count >= 2 && vertex_count % 2 == 0;
    plan.transformed = fvf == kD3dFvfTlVertex;
    const bool untransformed = fvf == kD3dFvfVertex || fvf == kD3dFvfLVertex;
    plan.vertex_stride = plan.transformed ? static_cast<std::uint32_t>(graphics::kTransformedLitVertexStride)
                                          : graphics::VertexStrideFromFvf(fvf);
    if ((!strip && !list && !lines) || (!plan.transformed && !untransformed) ||
        !graphics::AreLegacyDrawFlagsSupported(flags) || plan.vertex_stride == 0 ||
        vertex_count > 0xFFFFFFFFU / plan.vertex_stride)
    {
        plan.result = kDdErrUnsupported;
        return plan;
    }
    plan.topology = lines  ? graphics::PrimitiveTopology::kLineList
                    : list ? graphics::PrimitiveTopology::kTriangleList
                           : graphics::PrimitiveTopology::kTriangleStrip;
    return plan;
}

bool BuildFixedFunctionState(const DeviceState& device, graphics::LegacyFixedFunctionState* state, std::string* error)
{
    const auto& stage = device.texture_stage_states[0];
    const auto& render = device.render_states;
    if (stage[kD3dTssColorOp] != kD3dTopModulate || stage[kD3dTssColorArg1] != kD3dTaTexture ||
        stage[kD3dTssColorArg2] != kD3dTaDiffuse)
    {
        *error = "unsupported Direct3D3 texture color operation";
        return false;
    }
    switch (render[kD3dRenderStateCullMode])
    {
    case kD3dCullNone:
        state->cull_mode = graphics::CullMode::kNone;
        break;
    case kD3dCullCw:
        state->cull_mode = graphics::CullMode::kClockwise;
        break;
    case kD3dCullCcw:
        state->cull_mode = graphics::CullMode::kCounterClockwise;
        break;
    default:
        *error = "unsupported Direct3D3 cull mode";
        return false;
    }
    state->color_key_enabled = render[kD3dRenderStateColorKeyEnable] != 0;
    state->alpha_test_enabled = render[kD3dRenderStateAlphaTestEnable] != 0;
    state->alpha_reference = static_cast<std::uint8_t>(render[kD3dRenderStateAlphaRef] & 0xff);
    if (state->alpha_test_enabled && render[kD3dRenderStateAlphaFunc] != kD3dCmpNotEqual)
    {
        *error = "unsupported Direct3D3 alpha comparison function";
        return false;
    }
    state->alpha_function = CompareFunction::kNotEqual;
    state->alpha_blend_enabled = render[kD3dRenderStateAlphaBlendEnable] != 0;
    state->depth_test_enabled = render[kD3dRenderStateZEnable] != 0;
    state->depth_write_enabled = render[kD3dRenderStateZWriteEnable] != 0;
    if (state->depth_test_enabled && !ConvertCompare(render[kD3dRenderStateZFunc], &state->depth_function))
    {
        *error = "unsupported Direct3D3 depth comparison function";
        return false;
    }
    if (state->alpha_blend_enabled &&
        (!graphics::DecodeLegacyBlendFactor(render[kD3dRenderStateSrcBlend], &state->source_blend) ||
         !graphics::DecodeLegacyBlendFactor(render[kD3dRenderStateDestBlend], &state->destination_blend)))
    {
        *error = "unsupported Direct3D3 alpha blend factor";
        return false;
    }
    if (!ConvertFilter(stage[kD3dTssMinFilter], &state->minification_filter) ||
        !ConvertFilter(stage[kD3dTssMagFilter], &state->magnification_filter))
    {
        *error = "unsupported Direct3D3 texture filter";
        return false;
    }
    if (!ConvertAddress(stage[kD3dTssAddressU], &state->address_u) ||
        !ConvertAddress(stage[kD3dTssAddressV], &state->address_v))
    {
        *error = "unsupported Direct3D3 texture address mode";
        return false;
    }
    error->clear();
    return true;
}

std::uint32_t UntransformedVertexColor(const DeviceState& device)
{
    if (device.render_states[kD3dRenderStateLighting] == 0)
    {
        return 0xffffffffU;
    }
    const std::uint32_t ambient = device.render_states[kD3dRenderStateAmbient];
    const D3dMaterial7& material = device.material;
    const auto channel = [](float value) {
        // NaN clamps to 0 as well.
        const float clamped = value > 0.0f ? (std::min)(value, 1.0f) : 0.0f;
        return static_cast<std::uint32_t>(std::lround(clamped * 255.0f));
    };
    const auto ambient_of = [ambient](int shift) {
        return static_cast<float>((ambient >> shift) & 0xffU) / 255.0f;
    };
    const std::uint32_t red = channel(material.emissive.r + ambient_of(16) * material.ambient.r);
    const std::uint32_t green = channel(material.emissive.g + ambient_of(8) * material.ambient.g);
    const std::uint32_t blue = channel(material.emissive.b + ambient_of(0) * material.ambient.b);
    const std::uint32_t alpha = channel(material.diffuse.a);
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

bool BuildTransformState(const DeviceState& device, graphics::LegacyTransformState* transform, std::string* error)
{
    if (!device.has_viewport)
    {
        *error = "untransformed draw has no current viewport";
        return false;
    }
    transform->vertex_color = UntransformedVertexColor(device);
    CopyMatrix(device.transforms[kD3dTransformWorld], &transform->world);
    CopyMatrix(device.transforms[kD3dTransformView], &transform->view);
    CopyMatrix(device.transforms[kD3dTransformProjection], &transform->projection);
    // DirectX 7 has no clip volume on the viewport: the projection matrix
    // already produces normalized device coordinates, which is what the clip
    // defaults of LegacyViewportTransform describe. Only the screen rectangle
    // and depth range come from the guest.
    const D3dViewport7& source = device.viewport;
    transform->viewport.screen_x = static_cast<float>(source.x);
    transform->viewport.screen_y = static_cast<float>(source.y);
    transform->viewport.screen_width = static_cast<float>(source.width);
    transform->viewport.screen_height = static_cast<float>(source.height);
    transform->viewport.min_z = source.min_z;
    transform->viewport.max_z = source.max_z;
    error->clear();
    return true;
}

bool BuildViewport2TransformState(const DeviceState& device,
                                  const D3dViewport2& viewport,
                                  graphics::LegacyTransformState* transform,
                                  std::string* error)
{
    transform->vertex_color = UntransformedVertexColor(device);
    CopyMatrix(device.transforms[kD3dTransformWorld], &transform->world);
    CopyMatrix(device.transforms[kD3dTransformView], &transform->view);
    CopyMatrix(device.transforms[kD3dTransformProjection], &transform->projection);
    transform->viewport.screen_x = static_cast<float>(viewport.x);
    transform->viewport.screen_y = static_cast<float>(viewport.y);
    transform->viewport.screen_width = static_cast<float>(viewport.width);
    transform->viewport.screen_height = static_cast<float>(viewport.height);
    transform->viewport.clip_x = viewport.clip_x;
    transform->viewport.clip_y = viewport.clip_y;
    transform->viewport.clip_width = viewport.clip_width;
    transform->viewport.clip_height = viewport.clip_height;
    transform->viewport.min_z = viewport.min_z;
    transform->viewport.max_z = viewport.max_z;
    error->clear();
    return true;
}

void ApplyFadeCompatibility(const DeviceState& device,
                            const graphics::LegacyDrawCommand& command,
                            bool textured,
                            std::uint32_t logical_width,
                            std::uint32_t logical_height,
                            graphics::LegacyFixedFunctionState* state)
{
    // The factors the guest set, whether or not it also enabled blending. A
    // guest that named a blend and then drew a full-screen black quad without
    // enabling it is fading; honouring its own factors is what reproduces the
    // fade it asked for. 1st SE names ZERO/SRCALPHA, which scales the
    // framebuffer by the quad's alpha - forcing SRCALPHA/INVSRCALPHA there
    // gives dst*(1-a) instead of dst*a and runs the fade backwards.
    BlendFactor guest_source_blend = {};
    BlendFactor guest_destination_blend = {};
    const bool guest_blend_is_explicit =
        graphics::DecodeLegacyBlendFactor(device.render_states[kD3dRenderStateSrcBlend], &guest_source_blend) &&
        graphics::DecodeLegacyBlendFactor(device.render_states[kD3dRenderStateDestBlend], &guest_destination_blend);
    if (state->alpha_blend_enabled || state->alpha_test_enabled ||
        !IsFullScreenBlackFadeCandidate(command, textured, logical_width, logical_height, guest_blend_is_explicit))
    {
        return;
    }
    state->alpha_blend_enabled = true;
    if (guest_blend_is_explicit)
    {
        state->source_blend = guest_source_blend;
        state->destination_blend = guest_destination_blend;
    }
    else
    {
        state->source_blend = BlendFactor::kSourceAlpha;
        state->destination_blend = BlendFactor::kInverseSourceAlpha;
    }
    state->fade_compatibility_applied = true;
}

std::uint16_t Rgb565FromD3dColor(std::uint32_t color)
{
    const auto red = static_cast<std::uint16_t>((color >> 16) & 0xff);
    const auto green = static_cast<std::uint16_t>((color >> 8) & 0xff);
    const auto blue = static_cast<std::uint16_t>(color & 0xff);
    return static_cast<std::uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}

}  // namespace re2dj::directx
