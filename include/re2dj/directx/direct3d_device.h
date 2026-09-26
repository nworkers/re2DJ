#ifndef RE2DJ_DIRECTX_DIRECT3D_DEVICE_H_
#define RE2DJ_DIRECTX_DIRECT3D_DEVICE_H_

#include <array>
#include <cstdint>

#include "re2dj/directx/abi.h"

// Direct3D devices as the DirectX HLE models them: which devices
// CreateDevice makes, the state a device starts with, and the rules its
// state methods follow. Both hosts' facades keep a DeviceState per device;
// what a draw makes of the state is each host's own.
namespace re2dj::directx
{

inline constexpr std::uint32_t kRenderStateCount = 256;
inline constexpr std::uint32_t kLightStateCount = 256;
inline constexpr std::uint32_t kTextureStageCount = 8;
inline constexpr std::uint32_t kTextureStageStateCount = 64;
inline constexpr std::uint32_t kTransformStateCount = 32;

// A device's state, indexed by the guest's own state numbers.
struct DeviceState
{
    std::array<std::uint32_t, kRenderStateCount> render_states{};
    std::array<std::uint32_t, kLightStateCount> light_states{};
    std::array<std::array<std::uint32_t, kTextureStageStateCount>, kTextureStageCount> texture_stage_states{};
    std::array<D3dMatrix, kTransformStateCount> transforms{};
    D3dMaterial7 material;
    // DirectX 7 sets the viewport on the device; there is none until then.
    D3dViewport7 viewport;
    bool has_viewport = false;
    bool scene_active = false;
};

D3dMatrix IdentityMatrix();

// IDirect3D7::CreateDevice's rules: the device class must be one the
// enumeration reports (RGB, HAL, or T&L HAL; all three are the same device
// here), and the render target a surface with DDSCAPS_3DDEVICE; either
// failing is DDERR_INVALIDOBJECT.
bool IsEnumeratedDevice(const Guid& device_class);
std::uint32_t CheckCreateDevice(const Guid& device_class, std::uint32_t render_target_caps);

// A new device's state: counter-clockwise culling, ONE/ZERO blending, stage 0
// modulating the texture by the diffuse colour with point filtering and
// wrapping, identity world, view, and projection, and no viewport.
DeviceState InitialDeviceState();

// The state methods. An index outside its table is DDERR_INVALIDPARAMS; the
// facade checks the guest's pointers before calling these.
std::uint32_t SetRenderState(DeviceState& device, std::uint32_t state, std::uint32_t value);
std::uint32_t GetRenderState(const DeviceState& device, std::uint32_t state, std::uint32_t* value);
std::uint32_t SetLightState(DeviceState& device, std::uint32_t state, std::uint32_t value);
std::uint32_t GetLightState(const DeviceState& device, std::uint32_t state, std::uint32_t* value);
std::uint32_t SetTextureStageState(DeviceState& device,
                                   std::uint32_t stage,
                                   std::uint32_t state,
                                   std::uint32_t value);
std::uint32_t GetTextureStageState(const DeviceState& device,
                                   std::uint32_t stage,
                                   std::uint32_t state,
                                   std::uint32_t* value);
std::uint32_t SetTransform(DeviceState& device, std::uint32_t state, const D3dMatrix& matrix);
std::uint32_t GetTransform(const DeviceState& device, std::uint32_t state, D3dMatrix* matrix);

// A scene cannot begin inside another or end outside one
// (D3DERR_SCENE_IN_SCENE, D3DERR_SCENE_NOT_IN_SCENE).
std::uint32_t BeginScene(DeviceState& device);
std::uint32_t EndScene(DeviceState& device);

// A viewport needs a width and a height; asking for one before any is set is
// DDERR_NOTFOUND.
std::uint32_t SetViewport(DeviceState& device, const D3dViewport7& viewport);
std::uint32_t GetViewport(const DeviceState& device, D3dViewport7* viewport);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECT3D_DEVICE_H_
