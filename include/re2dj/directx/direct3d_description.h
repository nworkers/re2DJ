#ifndef RE2DJ_DIRECTX_DIRECT3D_DESCRIPTION_H_
#define RE2DJ_DIRECTX_DIRECT3D_DESCRIPTION_H_

#include <span>
#include <string_view>

#include "re2dj/directx/abi.h"

// What the Direct3D HLE says about itself: the devices it enumerates, their
// capabilities, and the depth and texture formats. Both hosts' facades answer
// from here. DirectDraw's side is directdraw_description.h.
namespace re2dj::directx
{

// One device IDirect3D7::EnumDevices reports.
struct Direct3DDevice
{
    std::string_view description;
    std::string_view name;
    Guid guid;
    bool hardware_transform_and_light;
};

// The devices in the order DirectX 7 enumerates them: the software rasterizer
// first and the most capable hardware device last, as titles commonly pick by
// walking that order. The names match the retail DirectX 7 strings because
// guests are known to select a device by comparing them.
std::span<const Direct3DDevice> Direct3D7Devices();

// A device's D3DDEVICEDESC7. Every capability the legacy OpenGL backend can
// honour is reported, since a guest that walks these fields rejects a device
// whose caps are left zero.
D3dDeviceDesc7 DeviceDescription(const Guid& device_guid, bool hardware_transform_and_light);
// IDirect3DDevice7::GetCaps: the HAL device the enumeration published.
D3dDeviceDesc7 CreatedDeviceDescription();

// The one depth format IDirect3D7::EnumZBufferFormats offers: 16-bit.
DdPixelFormat Depth16Format();
// The one texture format the shared surface backing stores: RGB565.
DdPixelFormat Rgb565Format();

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECT3D_DESCRIPTION_H_
