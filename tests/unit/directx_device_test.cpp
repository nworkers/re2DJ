#include "re2dj/directx/direct3d_device.h"

#include <cstdint>

#include "test_support.h"

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

}  // namespace

void RunDirectXDeviceTests(re2dj::test::Context& context)
{
    CheckCreateDeviceRules(context);
    CheckInitialState(context);
    CheckStateRules(context);
}
