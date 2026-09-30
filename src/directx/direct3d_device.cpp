#include "re2dj/directx/direct3d_device.h"

namespace re2dj::directx
{

D3dMatrix IdentityMatrix()
{
    D3dMatrix matrix;
    matrix.values[0] = 1.0f;
    matrix.values[5] = 1.0f;
    matrix.values[10] = 1.0f;
    matrix.values[15] = 1.0f;
    return matrix;
}

bool IsEnumeratedDevice(const Guid& device_class)
{
    return device_class == kIidDirect3DHalDevice || device_class == kIidDirect3DRgbDevice ||
           device_class == kIidDirect3DTnLHalDevice;
}

std::uint32_t CheckCreateDevice(const Guid& device_class, std::uint32_t render_target_caps)
{
    if (!IsEnumeratedDevice(device_class) || (render_target_caps & kDdsCaps3dDevice) == 0)
    {
        return kDdErrInvalidObject;
    }
    return kDdOk;
}

DeviceState InitialDeviceState()
{
    DeviceState device;
    device.render_states[kD3dRenderStateZWriteEnable] = 1;
    device.render_states[kD3dRenderStateZFunc] = kD3dCmpLessEqual;
    device.render_states[kD3dRenderStateAlphaFunc] = kD3dCmpAlways;
    device.render_states[kD3dRenderStateCullMode] = kD3dCullCcw;
    device.render_states[kD3dRenderStateSrcBlend] = kD3dBlendOne;
    device.render_states[kD3dRenderStateDestBlend] = kD3dBlendZero;
    auto& stage0 = device.texture_stage_states[0];
    stage0[kD3dTssColorOp] = kD3dTopModulate;
    stage0[kD3dTssColorArg1] = kD3dTaTexture;
    stage0[kD3dTssColorArg2] = kD3dTaDiffuse;
    stage0[kD3dTssMinFilter] = kD3dTfnPoint;
    stage0[kD3dTssMagFilter] = kD3dTfgPoint;
    stage0[kD3dTssAddressU] = kD3dTAddressWrap;
    stage0[kD3dTssAddressV] = kD3dTAddressWrap;
    device.transforms[kD3dTransformWorld] = IdentityMatrix();
    device.transforms[kD3dTransformView] = IdentityMatrix();
    device.transforms[kD3dTransformProjection] = IdentityMatrix();
    return device;
}

DeviceState InitialDevice7State()
{
    DeviceState device = InitialDeviceState();
    device.render_states[kD3dRenderStateLighting] = 1;
    return device;
}

std::uint32_t SetRenderState(DeviceState& device, std::uint32_t state, std::uint32_t value)
{
    if (state >= kRenderStateCount)
    {
        return kDdErrInvalidParams;
    }
    device.render_states[state] = value;
    return kDdOk;
}

std::uint32_t GetRenderState(const DeviceState& device, std::uint32_t state, std::uint32_t* value)
{
    if (state >= kRenderStateCount)
    {
        return kDdErrInvalidParams;
    }
    *value = device.render_states[state];
    return kDdOk;
}

std::uint32_t SetLightState(DeviceState& device, std::uint32_t state, std::uint32_t value)
{
    if (state >= kLightStateCount)
    {
        return kDdErrInvalidParams;
    }
    device.light_states[state] = value;
    return kDdOk;
}

std::uint32_t GetLightState(const DeviceState& device, std::uint32_t state, std::uint32_t* value)
{
    if (state >= kLightStateCount)
    {
        return kDdErrInvalidParams;
    }
    *value = device.light_states[state];
    return kDdOk;
}

std::uint32_t SetTextureStageState(DeviceState& device,
                                   std::uint32_t stage,
                                   std::uint32_t state,
                                   std::uint32_t value)
{
    if (stage >= kTextureStageCount || state >= kTextureStageStateCount)
    {
        return kDdErrInvalidParams;
    }
    device.texture_stage_states[stage][state] = value;
    return kDdOk;
}

std::uint32_t GetTextureStageState(const DeviceState& device,
                                   std::uint32_t stage,
                                   std::uint32_t state,
                                   std::uint32_t* value)
{
    if (stage >= kTextureStageCount || state >= kTextureStageStateCount)
    {
        return kDdErrInvalidParams;
    }
    *value = device.texture_stage_states[stage][state];
    return kDdOk;
}

std::uint32_t SetTransform(DeviceState& device, std::uint32_t state, const D3dMatrix& matrix)
{
    if (state >= kTransformStateCount)
    {
        return kDdErrInvalidParams;
    }
    device.transforms[state] = matrix;
    return kDdOk;
}

std::uint32_t GetTransform(const DeviceState& device, std::uint32_t state, D3dMatrix* matrix)
{
    if (state >= kTransformStateCount)
    {
        return kDdErrInvalidParams;
    }
    *matrix = device.transforms[state];
    return kDdOk;
}

std::uint32_t BeginScene(DeviceState& device)
{
    if (device.scene_active)
    {
        return kD3dErrSceneInScene;
    }
    device.scene_active = true;
    return kDdOk;
}

std::uint32_t EndScene(DeviceState& device)
{
    if (!device.scene_active)
    {
        return kD3dErrSceneNotInScene;
    }
    device.scene_active = false;
    return kDdOk;
}

std::uint32_t SetViewport(DeviceState& device, const D3dViewport7& viewport)
{
    if (viewport.width == 0 || viewport.height == 0)
    {
        return kDdErrInvalidParams;
    }
    device.viewport = viewport;
    device.has_viewport = true;
    return kDdOk;
}

std::uint32_t GetViewport(const DeviceState& device, D3dViewport7* viewport)
{
    if (!device.has_viewport)
    {
        return kDdErrNotFound;
    }
    *viewport = device.viewport;
    return kDdOk;
}

}  // namespace re2dj::directx
