#ifndef RE2DJ_DIRECTX_DIRECT3D_DRAW_H_
#define RE2DJ_DIRECTX_DIRECT3D_DRAW_H_

#include <cstdint>
#include <string>

#include "re2dj/directx/direct3d_device.h"
#include "re2dj/graphics/legacy_draw_command.h"
#include "re2dj/graphics/legacy_texture.h"
#include "re2dj/graphics/legacy_transform.h"

// What a Direct3D draw means to the shared render backend: which draws are
// modelled, the fixed-function state a device's state becomes, the transform
// of untransformed vertices, and the fade compatibility the Windows facade
// has always applied. Both hosts draw through these rules.
namespace re2dj::directx
{

// The draws the facade renders: triangle strips (3+ vertices), triangle
// lists and line lists (whole primitives), of transformed-and-lit vertices
// (D3DFVF_TLVERTEX) or of D3DFVF_VERTEX / D3DFVF_LVERTEX transformed by the
// device. Anything else is DDERR_UNSUPPORTED.
struct DrawPlan
{
    std::uint32_t result = kDdOk;
    graphics::PrimitiveTopology topology = graphics::PrimitiveTopology::kTriangleStrip;
    bool transformed = false;
    std::uint32_t vertex_stride = 0;
};
DrawPlan PlanDrawPrimitive(std::uint32_t primitive, std::uint32_t fvf, std::uint32_t vertex_count, std::uint32_t flags);

// The backend state for a device's render and texture stage states; false
// with error for a state the backend does not model (a texture operation
// other than stage 0 modulating texture by diffuse, an alpha test other than
// NOTEQUAL, an unknown cull mode, comparison, blend factor, filter, or
// address mode).
bool BuildFixedFunctionState(const DeviceState& device, graphics::LegacyFixedFunctionState* state, std::string* error);

// The colour an untransformed D3DVERTEX, which carries none of its own, is
// drawn with. With D3DRENDERSTATE_LIGHTING off it is opaque white. With it on
// and no light, as measured on a Windows 11 Direct3D 7 HAL device (task 430):
// each of red, green and blue is emissive + ambient render state x material
// ambient, and alpha the material's diffuse alpha, each clamped to 0..1 and
// rounded to 8 bits. Lights are not modelled.
std::uint32_t UntransformedVertexColor(const DeviceState& device);

// The transform of untransformed vertices from the device's matrices and its
// DirectX 7 viewport, with UntransformedVertexColor; false with error when no
// viewport has been set.
bool BuildTransformState(const DeviceState& device, graphics::LegacyTransformState* transform, std::string* error);

// The same from a DirectX 6 viewport object's D3DVIEWPORT2, whose clip
// volume the guest gives as well; it takes the place of the device's
// DirectX 7 viewport, as the Windows DX6 facade reads it at each draw.
bool BuildViewport2TransformState(const DeviceState& device,
                                  const D3dViewport2& viewport,
                                  graphics::LegacyTransformState* transform,
                                  std::string* error);

// A full-screen, untextured, black four-vertex strip whose alpha is neither 0
// nor, unless the guest named its blend factors, 0xff: the guest is fading
// the screen without enabling blending. The facade then blends it anyway,
// with the guest's own factors when it named them and SRCALPHA/INVSRCALPHA
// otherwise, as the Windows facade always has.
void ApplyFadeCompatibility(const DeviceState& device,
                            const graphics::LegacyDrawCommand& command,
                            bool textured,
                            std::uint32_t logical_width,
                            std::uint32_t logical_height,
                            graphics::LegacyFixedFunctionState* state);

// A D3DCOLOR's 5-6-5 value, as a render target clear stores it.
std::uint16_t Rgb565FromD3dColor(std::uint32_t color);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECT3D_DRAW_H_
