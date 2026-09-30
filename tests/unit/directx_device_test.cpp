#include "re2dj/directx/direct3d_description.h"
#include "re2dj/directx/direct3d_device.h"
#include "re2dj/directx/direct3d_draw.h"
#include "re2dj/directx/direct3d_vertex_buffer.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "test_support.h"

#if defined(_WIN32)
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <d3d.h>
#pragma comment(lib, "dxguid.lib")
#endif

namespace
{

namespace dx = re2dj::directx;

// The enumerated classes make devices on a 3D surface; anything else is
// DDERR_INVALIDOBJECT.
void CheckCreateDeviceRules(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, dx::CheckCreateDevice(dx::kIidDirect3DHalDevice, dx::kDdsCaps3dDevice), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::CheckCreateDevice(dx::kIidDirect3DRgbDevice, dx::kDdsCaps3dDevice), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::CheckCreateDevice(dx::kIidDirect3DTnLHalDevice, dx::kDdsCaps3dDevice), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::CheckCreateDevice(dx::kIidDirectDraw7, dx::kDdsCaps3dDevice), dx::kDdErrInvalidObject);
    RE2DJ_CHECK_EQ(context, dx::CheckCreateDevice(dx::kIidDirect3DHalDevice, dx::kDdsCapsTexture),
                   dx::kDdErrInvalidObject);
}

// A new device's state, as the Windows facade set it up.
void CheckInitialState(re2dj::test::Context& context)
{
    const dx::DeviceState device = dx::InitialDeviceState();
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateCullMode], dx::kD3dCullCcw);
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateSrcBlend], dx::kD3dBlendOne);
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateDestBlend], dx::kD3dBlendZero);
    RE2DJ_CHECK_EQ(context, device.texture_stage_states[0][dx::kD3dTssColorOp], dx::kD3dTopModulate);
    RE2DJ_CHECK_EQ(context, device.texture_stage_states[0][dx::kD3dTssColorArg1], dx::kD3dTaTexture);
    RE2DJ_CHECK_EQ(context, device.texture_stage_states[0][dx::kD3dTssAddressV], dx::kD3dTAddressWrap);
    RE2DJ_CHECK_EQ(context, device.texture_stage_states[1][dx::kD3dTssColorOp], 0U);
    RE2DJ_CHECK(context, device.transforms[dx::kD3dTransformProjection].values == dx::IdentityMatrix().values);
    RE2DJ_CHECK_EQ(context, device.transforms[0].values[0], 0.0f);
    RE2DJ_CHECK(context, !device.has_viewport);
    RE2DJ_CHECK(context, !device.scene_active);
    // The render states a new Direct3D 3 and Direct3D 7 HAL device report on
    // Windows 11 (task 430): depth writes on, LESSEQUAL, and ALWAYS for the
    // alpha test; lighting only on a Direct3D 7 device; a zero material.
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateZEnable], 0U);
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateZWriteEnable], 1U);
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateZFunc], dx::kD3dCmpLessEqual);
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateAlphaFunc], dx::kD3dCmpAlways);
    RE2DJ_CHECK_EQ(context, device.render_states[dx::kD3dRenderStateLighting], 0U);
    RE2DJ_CHECK_EQ(context, device.material.diffuse.a, 0.0f);
    const dx::DeviceState device7 = dx::InitialDevice7State();
    RE2DJ_CHECK_EQ(context, device7.render_states[dx::kD3dRenderStateLighting], 1U);
    RE2DJ_CHECK_EQ(context, device7.render_states[dx::kD3dRenderStateZFunc], dx::kD3dCmpLessEqual);
    RE2DJ_CHECK_EQ(context, device7.render_states[dx::kD3dRenderStateAmbient], 0U);
}

// The colour a D3DVERTEX is drawn with, against the pixels a Windows 11
// Direct3D 7 HAL device drew for the same material and ambient (task 430).
void CheckUntransformedVertexColor(re2dj::test::Context& context)
{
    dx::DeviceState device = dx::InitialDevice7State();
    const auto material = [&](float ar, float ag, float ab, float aa, float da, float er, float eg, float eb) {
        device.material = {};
        device.material.ambient = {ar, ag, ab, aa};
        device.material.diffuse = {0.9f, 0.9f, 0.9f, da};
        device.material.emissive = {er, eg, eb, 1.0f};
    };
    // No material: black and fully transparent, whatever the ambient.
    device.render_states[dx::kD3dRenderStateAmbient] = 0xffffffffU;
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0x00000000U);
    // Lighting off: opaque white.
    device.render_states[dx::kD3dRenderStateLighting] = 0;
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xffffffffU);
    device.render_states[dx::kD3dRenderStateLighting] = 1;
    material(0.5f, 0.25f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xff8040ffU);
    device.render_states[dx::kD3dRenderStateAmbient] = 0x00808080U;
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xff402080U);
    device.render_states[dx::kD3dRenderStateAmbient] = 0x00ff0000U;
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xff800000U);
    device.render_states[dx::kD3dRenderStateAmbient] = 0xffffffffU;
    material(0.5f, 0.25f, 1.0f, 1.0f, 1.0f, 0.25f, 0.5f, 0.0f);
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xffbfbfffU);
    // The sum clamps at 1, and so does a diffuse alpha outside 0..1.
    material(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xffffffffU);
    material(-0.5f, 2.0f, 0.5f, 1.0f, 1.5f, 0.0f, 0.0f, 0.0f);
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device), 0xff00ff80U);
    // Alpha is the diffuse alpha; the ambient alpha plays no part.
    material(1.0f, 1.0f, 1.0f, 0.25f, 0.5f, 0.0f, 0.0f, 0.0f);
    RE2DJ_CHECK_EQ(context, dx::UntransformedVertexColor(device) >> 24, 0x80U);
    // The transform carries it to a D3DVERTEX draw.
    dx::D3dViewport7 viewport;
    viewport.width = 640;
    viewport.height = 480;
    RE2DJ_CHECK_EQ(context, dx::SetViewport(device, viewport), dx::kDdOk);
    re2dj::graphics::LegacyTransformState transform;
    std::string error;
    RE2DJ_CHECK(context, dx::BuildTransformState(device, &transform, &error));
    RE2DJ_CHECK_EQ(context, transform.vertex_color, dx::UntransformedVertexColor(device));
}

// Index limits, scenes, and the viewport.
void CheckStateRules(re2dj::test::Context& context)
{
    dx::DeviceState device = dx::InitialDeviceState();
    std::uint32_t value = 0;
    RE2DJ_CHECK_EQ(context, dx::SetRenderState(device, 255, 7), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::GetRenderState(device, 255, &value), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, value, 7U);
    RE2DJ_CHECK_EQ(context, dx::SetRenderState(device, 256, 7), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::SetLightState(device, 256, 1), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::SetTextureStageState(device, 7, 63, 1), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::SetTextureStageState(device, 8, 0, 1), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::SetTextureStageState(device, 0, 64, 1), dx::kDdErrInvalidParams);
    dx::D3dMatrix matrix;
    RE2DJ_CHECK_EQ(context, dx::SetTransform(device, 31, matrix), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::GetTransform(device, 32, &matrix), dx::kDdErrInvalidParams);

    RE2DJ_CHECK_EQ(context, dx::EndScene(device), dx::kD3dErrSceneNotInScene);
    RE2DJ_CHECK_EQ(context, dx::BeginScene(device), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::BeginScene(device), dx::kD3dErrSceneInScene);
    RE2DJ_CHECK_EQ(context, dx::EndScene(device), dx::kDdOk);

    dx::D3dViewport7 viewport;
    RE2DJ_CHECK_EQ(context, dx::GetViewport(device, &viewport), dx::kDdErrNotFound);
    viewport.width = 640;
    RE2DJ_CHECK_EQ(context, dx::SetViewport(device, viewport), dx::kDdErrInvalidParams);
    viewport.height = 480;
    RE2DJ_CHECK_EQ(context, dx::SetViewport(device, viewport), dx::kDdOk);
    dx::D3dViewport7 read;
    RE2DJ_CHECK_EQ(context, dx::GetViewport(device, &read), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, read.height, 480U);
}

// The draws the core plans: strips of 3+, whole lists and line lists, of the
// three vertex formats; anything else is DDERR_UNSUPPORTED.
void CheckDrawPlan(re2dj::test::Context& context)
{
    dx::DrawPlan plan = dx::PlanDrawPrimitive(dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, 4, 0);
    RE2DJ_CHECK_EQ(context, plan.result, dx::kDdOk);
    RE2DJ_CHECK(context, plan.transformed);
    RE2DJ_CHECK_EQ(context, plan.vertex_stride, 32U);
    RE2DJ_CHECK(context, plan.topology == re2dj::graphics::PrimitiveTopology::kTriangleStrip);
    plan = dx::PlanDrawPrimitive(dx::kD3dPtTriangleList, dx::kD3dFvfLVertex, 6, 0);
    RE2DJ_CHECK_EQ(context, plan.result, dx::kDdOk);
    RE2DJ_CHECK(context, !plan.transformed);
    RE2DJ_CHECK_EQ(context, plan.vertex_stride, 32U);
    RE2DJ_CHECK(context, plan.topology == re2dj::graphics::PrimitiveTopology::kTriangleList);
    plan = dx::PlanDrawPrimitive(dx::kD3dPtLineList, dx::kD3dFvfVertex, 2, 0);
    RE2DJ_CHECK_EQ(context, plan.result, dx::kDdOk);
    RE2DJ_CHECK(context, plan.topology == re2dj::graphics::PrimitiveTopology::kLineList);

    RE2DJ_CHECK_EQ(context, dx::PlanDrawPrimitive(dx::kD3dPtTriangleStrip, dx::kD3dFvfTlVertex, 2, 0).result,
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, dx::PlanDrawPrimitive(dx::kD3dPtTriangleList, dx::kD3dFvfTlVertex, 4, 0).result,
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, dx::PlanDrawPrimitive(dx::kD3dPtLineList, dx::kD3dFvfTlVertex, 3, 0).result,
                   dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, dx::PlanDrawPrimitive(1, dx::kD3dFvfTlVertex, 4, 0).result, dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, dx::PlanDrawPrimitive(dx::kD3dPtTriangleStrip, 0x44, 4, 0).result,
                   dx::kDdErrUnsupported);
}

// The fixed-function state a device's state becomes, and the states the
// backend does not model.
void CheckFixedFunctionState(re2dj::test::Context& context)
{
    dx::DeviceState device = dx::InitialDeviceState();
    re2dj::graphics::LegacyFixedFunctionState state;
    std::string error;
    RE2DJ_CHECK(context, dx::BuildFixedFunctionState(device, &state, &error));
    RE2DJ_CHECK(context, state.cull_mode == re2dj::graphics::CullMode::kCounterClockwise);
    RE2DJ_CHECK(context, !state.alpha_blend_enabled);

    dx::SetRenderState(device, dx::kD3dRenderStateCullMode, dx::kD3dCullNone);
    dx::SetRenderState(device, dx::kD3dRenderStateAlphaBlendEnable, 1);
    dx::SetRenderState(device, dx::kD3dRenderStateSrcBlend, 5);
    dx::SetRenderState(device, dx::kD3dRenderStateDestBlend, 6);
    dx::SetRenderState(device, dx::kD3dRenderStateColorKeyEnable, 1);
    dx::SetTextureStageState(device, 0, dx::kD3dTssMagFilter, dx::kD3dTfnLinear);
    dx::SetTextureStageState(device, 0, dx::kD3dTssAddressU, dx::kD3dTAddressClamp);
    RE2DJ_CHECK(context, dx::BuildFixedFunctionState(device, &state, &error));
    RE2DJ_CHECK(context, state.cull_mode == re2dj::graphics::CullMode::kNone);
    RE2DJ_CHECK(context, state.alpha_blend_enabled && state.color_key_enabled);
    RE2DJ_CHECK(context, state.source_blend == re2dj::graphics::BlendFactor::kSourceAlpha);
    RE2DJ_CHECK(context, state.destination_blend == re2dj::graphics::BlendFactor::kInverseSourceAlpha);
    RE2DJ_CHECK(context, state.magnification_filter == re2dj::graphics::TextureFilter::kLinear);
    RE2DJ_CHECK(context, state.address_u == re2dj::graphics::TextureAddressMode::kClamp);

    dx::DeviceState refused = device;
    dx::SetRenderState(refused, dx::kD3dRenderStateCullMode, 9);
    RE2DJ_CHECK(context, !dx::BuildFixedFunctionState(refused, &state, &error));
    refused = device;
    dx::SetTextureStageState(refused, 0, dx::kD3dTssColorOp, 2);
    RE2DJ_CHECK(context, !dx::BuildFixedFunctionState(refused, &state, &error));
    refused = device;
    dx::SetRenderState(refused, dx::kD3dRenderStateAlphaTestEnable, 1);
    dx::SetRenderState(refused, dx::kD3dRenderStateAlphaFunc, dx::kD3dCmpAlways);
    RE2DJ_CHECK(context, !dx::BuildFixedFunctionState(refused, &state, &error));
    RE2DJ_CHECK(context, !error.empty());
}

// Untransformed vertices need a viewport; a full-screen black quad drawn
// without blending is blended as a fade; D3DCOLOR to 5-6-5.
void CheckTransformFadeAndColor(re2dj::test::Context& context)
{
    dx::DeviceState device = dx::InitialDeviceState();
    re2dj::graphics::LegacyTransformState transform;
    std::string error;
    RE2DJ_CHECK(context, !dx::BuildTransformState(device, &transform, &error));
    dx::D3dViewport7 viewport;
    viewport.x = 10;
    viewport.width = 640;
    viewport.height = 480;
    viewport.max_z = 1.0f;
    dx::SetViewport(device, viewport);
    RE2DJ_CHECK(context, dx::BuildTransformState(device, &transform, &error));
    RE2DJ_CHECK_EQ(context, transform.viewport.screen_x, 10.0f);
    RE2DJ_CHECK_EQ(context, transform.viewport.screen_width, 640.0f);

    re2dj::graphics::LegacyDrawCommand command;
    command.topology = re2dj::graphics::PrimitiveTopology::kTriangleStrip;
    const float corners[4][2] = {{0.0f, 480.0f}, {0.0f, 0.0f}, {640.0f, 480.0f}, {640.0f, 0.0f}};
    for (const auto& corner : corners)
    {
        re2dj::graphics::TransformedLitVertex vertex;
        vertex.x = corner[0];
        vertex.y = corner[1];
        vertex.diffuse_argb = 0x80000000U;
        command.vertices.push_back(vertex);
    }
    re2dj::graphics::LegacyFixedFunctionState state;
    dx::BuildFixedFunctionState(device, &state, &error);
    dx::ApplyFadeCompatibility(device, command, true, 640, 480, &state);
    RE2DJ_CHECK(context, !state.fade_compatibility_applied);
    dx::ApplyFadeCompatibility(device, command, false, 640, 480, &state);
    RE2DJ_CHECK(context, state.fade_compatibility_applied && state.alpha_blend_enabled);

    RE2DJ_CHECK_EQ(context, dx::Rgb565FromD3dColor(0x00FF0000U), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, dx::Rgb565FromD3dColor(0xFF00FF00U), std::uint16_t{0x07E0});
    RE2DJ_CHECK_EQ(context, dx::Rgb565FromD3dColor(0x000000FFU), std::uint16_t{0x001F});
}

// The vertex buffer rules, as the Windows facade answers.
void CheckVertexBufferRules(re2dj::test::Context& context)
{
    dx::D3dVertexBufferDesc description;
    description.size = 16;
    description.fvf = dx::kD3dFvfTlVertex;
    description.vertex_count = 4;
    std::uint32_t stride = 0;
    RE2DJ_CHECK_EQ(context, dx::CheckCreateVertexBuffer(true, true, description, &stride), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, stride, 32U);
    RE2DJ_CHECK_EQ(context, dx::CheckCreateVertexBuffer(false, true, description, &stride), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckCreateVertexBuffer(true, false, description, &stride), dx::kDdErrInvalidParams);
    description.size = 15;
    RE2DJ_CHECK_EQ(context, dx::CheckCreateVertexBuffer(true, true, description, &stride), dx::kDdErrInvalidParams);
    description.size = 16;
    description.fvf = 0;
    RE2DJ_CHECK_EQ(context, dx::CheckCreateVertexBuffer(true, true, description, &stride), dx::kDdErrInvalidParams);

    RE2DJ_CHECK_EQ(context, dx::CheckLockVertexBuffer(false, false, 128), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckLockVertexBuffer(true, true, 128), dx::kD3dErrVertexBufferLocked);
    RE2DJ_CHECK_EQ(context, dx::CheckLockVertexBuffer(true, false, 128), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::CheckUnlockVertexBuffer(false), dx::kDdErrNotLocked);
    RE2DJ_CHECK_EQ(context, dx::CheckGetVertexBufferDesc(true, 12), dx::kDdErrInvalidParams);

    RE2DJ_CHECK_EQ(context, dx::CheckDrawPrimitiveVB(4, false, 0, 4), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawPrimitiveVB(0, false, 0, 4), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawPrimitiveVB(4, true, 0, 4), dx::kD3dErrVertexBufferLocked);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawPrimitiveVB(4, false, 1, 4), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawPrimitiveVB(1, false, 5, 4), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawIndexedPrimitiveVB(1, true, 6, false), dx::kDdErrUnsupported);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawIndexedPrimitiveVB(0, false, 6, false), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawIndexedPrimitiveVB(0, true, 0, false), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, dx::CheckDrawIndexedPrimitiveVB(0, true, 6, true), dx::kD3dErrVertexBufferLocked);

#if defined(_WIN32)
    // The core's identifiers are the SDK's.
    RE2DJ_CHECK(context, std::memcmp(&IID_IDirect3DVertexBuffer7, dx::kIidDirect3DVertexBuffer7.data(), 16) == 0);
    RE2DJ_CHECK(context, std::memcmp(&IID_IDirect3DVertexBuffer, dx::kIidDirect3DVertexBuffer.data(), 16) == 0);
#endif
}

}  // namespace

// IDirect3D3::FindDevice as the DirectX 6 facade answers: sizes checked, a
// software search not found, otherwise the HAL device with its four fields.
void CheckFindDevice(re2dj::test::Context& context)
{
    dx::D3dFindDeviceSearch search;
    search.size = sizeof(search);
    dx::D3dFindDeviceResult result;
    result.size = sizeof(result);
    result.software.size = 99;
    RE2DJ_CHECK_EQ(context, dx::FindDevice(search, &result), dx::kDdOk);
    RE2DJ_CHECK(context, result.guid == dx::kIidDirect3DHalDevice);
    RE2DJ_CHECK_EQ(context, result.hardware.size, 252U);
    RE2DJ_CHECK_EQ(context, result.hardware.flags, 0x190U);
    RE2DJ_CHECK_EQ(context, result.hardware.clipping, 1U);
    RE2DJ_CHECK_EQ(context, result.hardware.render_bit_depth, dx::kDdbd16);
    RE2DJ_CHECK_EQ(context, result.hardware.z_buffer_bit_depth, dx::kDdbd16);
    RE2DJ_CHECK_EQ(context, result.software.size, 0U);
    search.flags = dx::kD3dFdsHardware;
    RE2DJ_CHECK_EQ(context, dx::FindDevice(search, &result), dx::kDdErrNotFound);
    search.hardware = 1;
    RE2DJ_CHECK_EQ(context, dx::FindDevice(search, &result), dx::kDdOk);
    search.size = 88;
    RE2DJ_CHECK_EQ(context, dx::FindDevice(search, &result), dx::kDdErrInvalidParams);
    search.size = sizeof(search);
    result.size = 0;
    RE2DJ_CHECK_EQ(context, dx::FindDevice(search, &result), dx::kDdErrInvalidParams);
}

// IDirect3DDevice3::GetCaps, IDirect3D3::EnumZBufferFormats, and a DirectX 6
// viewport's transform, as the Windows DX6 facade answers them.
void CheckDirectX6Descriptions(re2dj::test::Context& context)
{
    dx::D3dDeviceDesc6 hardware;
    dx::D3dDeviceDesc6 software;
    RE2DJ_CHECK_EQ(context, dx::GetDevice3Caps(nullptr, &software), dx::kDdErrInvalidParams);
    hardware.size = 100;
    RE2DJ_CHECK_EQ(context, dx::GetDevice3Caps(&hardware, nullptr), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, hardware.clipping, 0U);
    hardware.size = sizeof(hardware);
    RE2DJ_CHECK_EQ(context, dx::GetDevice3Caps(&hardware, nullptr), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, hardware.flags, 0x190U);
    RE2DJ_CHECK_EQ(context, hardware.clipping, 1U);
    RE2DJ_CHECK_EQ(context, hardware.render_bit_depth, dx::kDdbd16);
    // A software description of the wrong size refuses, the hardware one
    // already filled; of the right size it is zeroed.
    hardware = {};
    hardware.size = sizeof(hardware);
    software.size = 12;
    RE2DJ_CHECK_EQ(context, dx::GetDevice3Caps(&hardware, &software), dx::kDdErrInvalidParams);
    RE2DJ_CHECK_EQ(context, hardware.clipping, 1U);
    software.size = sizeof(software);
    software.clipping = 7;
    RE2DJ_CHECK_EQ(context, dx::GetDevice3Caps(&hardware, &software), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, software.size, 252U);
    RE2DJ_CHECK_EQ(context, software.clipping, 0U);

    RE2DJ_CHECK_EQ(context, dx::CheckEnumZBufferFormats3(dx::kIidDirect3DHalDevice), dx::kDdOk);
    RE2DJ_CHECK_EQ(context, dx::CheckEnumZBufferFormats3(dx::kIidDirect3DRgbDevice), dx::kDdErrInvalidParams);
    const dx::DdPixelFormat depth = dx::Direct3D3DepthFormat();
    RE2DJ_CHECK_EQ(context, depth.flags, dx::kDdpfZBuffer);
    RE2DJ_CHECK_EQ(context, depth.bit_count, 16U);
    RE2DJ_CHECK_EQ(context, depth.green_mask, 0U);

    // A viewport object's D3DVIEWPORT2 takes the device's place, clip volume
    // included; no DirectX 7 viewport is needed.
    const dx::DeviceState device = dx::InitialDeviceState();
    dx::D3dViewport2 viewport;
    viewport.size = sizeof(viewport);
    viewport.x = 4;
    viewport.width = 640;
    viewport.height = 480;
    viewport.clip_x = -1.0f;
    viewport.clip_y = 1.0f;
    viewport.clip_width = 2.0f;
    viewport.clip_height = 2.0f;
    viewport.max_z = 1.0f;
    re2dj::graphics::LegacyTransformState transform;
    std::string error;
    RE2DJ_CHECK(context, dx::BuildViewport2TransformState(device, viewport, &transform, &error));
    RE2DJ_CHECK_EQ(context, transform.viewport.screen_x, 4.0f);
    RE2DJ_CHECK_EQ(context, transform.viewport.screen_height, 480.0f);
    RE2DJ_CHECK_EQ(context, transform.viewport.clip_x, -1.0f);
    RE2DJ_CHECK_EQ(context, transform.viewport.clip_width, 2.0f);
    RE2DJ_CHECK_EQ(context, transform.viewport.max_z, 1.0f);
    // A DirectX 6 device has no lighting render state, so its D3DVERTEX stays
    // white.
    RE2DJ_CHECK_EQ(context, transform.vertex_color, 0xffffffffU);
}

void RunDirectXDeviceTests(re2dj::test::Context& context)
{
    CheckFindDevice(context);
    CheckDirectX6Descriptions(context);
    CheckCreateDeviceRules(context);
    CheckInitialState(context);
    CheckUntransformedVertexColor(context);
    CheckStateRules(context);
    CheckDrawPlan(context);
    CheckFixedFunctionState(context);
    CheckTransformFadeAndColor(context);
    CheckVertexBufferRules(context);
}
